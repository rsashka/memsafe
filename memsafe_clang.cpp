#include "memsafe_plugin.h"

#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/Diagnostic.h"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/CompilerInstance.h"

#include "clang/AST/AST.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Attr.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/IR/Attributes.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Lex/PreprocessorOptions.h"

#pragma clang attribute push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-this-capture"

#include "clang/AST/ASTDumper.h"

#pragma clang attribute pop

using namespace clang;
using namespace clang::ast_matchers;
using namespace memsafe;


namespace {


    /**
     * @def MemSafePlugin
     * 
     * The MemSafePlugin class is made as a RecursiveASTVisitor template for the following reasons:
     * - AST traversal occurs from top to bottom through all leaves, which allows to dynamically create and clear context information.
     * Whereas when searching for matches using AST Matcher, MatchCallback only called for the found nodes,
     * but clearing the necessary context information for each call is very expensive.
     * - RecursiveASTVisitor allows interrupting (or repeating) traversal of individual AST node depending on dynamic context information.
     * - Matcher processes AST only for specific specified matchers and analysis of missed (not processed) attributes is difficult for it,
     * whereas RecursiveASTVisitor sequentially traverses all AST nodes regardless of the configured templates.
     *
     * The plugin is used as a static signleton to store context and simplify access from any classes,
     * but is created dynamically to use the context CompilerInstance
     *
     * Traverse ... - can form the current context
     * Visit ... - only analyze data without changing the context
     * 
     */

    class MemSafePlugin;
    static std::unique_ptr<MemSafePlugin> plugin;
    static std::unique_ptr<MemSafeFile> scaner;

    /**
     * @def MemSafeLogger
     * 
     * Container for addeded attributes with a processing mark 
     * (so as not to miss unprocessed attributes) and plugin analyzer log.
     */

    class MemSafeLogger {
        const CompilerInstance &m_ci;
        std::map<SourceLocation, bool> m_attrs;
        std::vector<std::pair<SourceLocation, std::string>> m_logs;
    public:

        MemSafeLogger(const CompilerInstance &ci) : m_ci(ci) {
        }

        void Log(SourceLocation loc, std::string str) {
            m_logs.push_back({std::move(loc), std::move(str)});
        }

        inline void AttrAdd(SourceLocation loc) {
            assert(loc.isValid());
            m_attrs.emplace(loc, false);
        }

        void AttrComplete(SourceLocation loc) {
            auto found = m_attrs.find(loc);
            if (found == m_attrs.end()) {
                Log(loc, "Attribute location not found!");
            } else {
                found->second = true;
            }
        }

        static std::string LocToStr(const SourceLocation &loc, const SourceManager &sm) {
            std::string str = loc.printToString(sm);
            size_t pos = str.find(' ');
            if (pos == std::string::npos) {
                return str;
            }
            return str.substr(0, pos);
        }

        inline std::string LocToStr(const SourceLocation &loc) {
            return LocToStr(loc, m_ci.getSourceManager());
        }

        void Dump(raw_ostream & out) {

            // For simple copy-paste into unit tests

            //            for (auto &elem : m_logs) {
            //                out << "    \"" << elem.second.substr(0, elem.second.find(" ", 7)) << "\",\n";
            //            }
            //            out << "\n";


            out << MEMSAFE_KEYWORD_START_LOG;
            for (auto &elem : m_logs) {
                out << LocToStr(elem.first);
                out << ": " << elem.second << "\n";
            }
            for (auto &elem : m_attrs) {
                if (!elem.second) {
                    out << LocToStr(elem.first);
                    out << ": unprocessed attribute!\n";
                }
            }
        }
    };

    static std::unique_ptr<MemSafeLogger> logger;

    /**
     * @def MemSafeAttrInfo
     * 
     * Class for applies a custom attribute to any declaration 
     * or expression without examining the arguments 
     * (only the number of arguments and their type are checked).
     */

    struct MemSafeAttrInfo : public ParsedAttrInfo {

        MemSafeAttrInfo() {

            OptArgs = 3;

            static constexpr Spelling S[] = {
                {ParsedAttr::AS_GNU, TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE)},
                {ParsedAttr::AS_C23, TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE)},
                {ParsedAttr::AS_CXX11, TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE)},
                {ParsedAttr::AS_CXX11, "::" TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE)},
            };
            Spellings = S;
        }

        AnnotateAttr * CreateAttr(Sema &S, const ParsedAttr &Attr) const {

            if (Attr.getNumArgs() < 1 || Attr.getNumArgs() > 3) {

                S.Diag(Attr.getLoc(), S.getDiagnostics().getCustomDiagID(
                        DiagnosticsEngine::Error,
                        "The attribute '" TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE) "' expects two or three string literals as arguments."));

                return nullptr;
            }

            SmallVector<Expr *, 4> ArgsBuf;
            for (unsigned i = 0; i < Attr.getNumArgs(); i++) {

                if (!dyn_cast<StringLiteral>(Attr.getArgAsExpr(i)->IgnoreParenCasts())) {
                    S.Diag(Attr.getLoc(), S.getDiagnostics().getCustomDiagID(
                            DiagnosticsEngine::Error, "Expected argument as a string literal"));
                    return nullptr;
                }

                ArgsBuf.push_back(Attr.getArgAsExpr(i));
            }

            // Add empty second argument
            if (Attr.getNumArgs() == 1) {
                ArgsBuf.push_back(StringLiteral::CreateEmpty(S.Context, 0, 0, 0));
            }


            // Add attribute location to check after plugin processing
            if (logger) {
                logger->AttrAdd(Attr.getLoc());
            }

            return AnnotateAttr::Create(S.Context, TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE), ArgsBuf.data(), ArgsBuf.size(), Attr.getRange());
        }

        AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &Attr) const override {
            if (auto attr = CreateAttr(S, Attr)) {
                D->addAttr(attr);
                return AttributeApplied;
            }
            return AttributeNotApplied;
        }

        AttrHandling handleStmtAttribute(Sema &S, Stmt *St, const ParsedAttr &Attr, class Attr *&Result) const override {
            if ((Result = CreateAttr(S, Attr))) {
                return AttributeApplied;
            }
            return AttributeNotApplied;
        }

    };

    /*
     * 
     * 
     * 
     */

    struct DeclInfo {
        const ValueDecl * decl;
        const char * type;
    };

    struct LifeTime {
        typedef std::variant<std::monostate, const FunctionDecl *, const CXXRecordDecl *, const CXXTemporaryObjectExpr *, const CallExpr *, const CXXMemberCallExpr *, const CXXOperatorCallExpr *, const MemberExpr *> ScopeType;
        /**
         * The scope and lifetime of variables (code block, function definition, function or method call, etc.)
         */
        const ScopeType scope;

        /** 
         * Map of iterators and reference variables (iter => value)
         */
        typedef std::map<std::string, std::string> DependentType;
        DependentType dependent;
        static inline const DependentType::iterator DependentEnd = static_cast<DependentType::iterator> (nullptr);

        /** 
         * Map of dependent variable usage and their possible changes 
         * (data modifications that can invalidate the iterator)
         * (value => Location)
         */
        typedef std::map<std::string, std::vector<SourceLocation>> BlockerType;
        BlockerType blocker;
        static inline const BlockerType::iterator BlockerEnd = static_cast<BlockerType::iterator> (nullptr);

        // List of traking variables
        std::map<const std::string, DeclInfo> vars;

        // List of un traking (other) variables
        std::set<std::string> other;

        // Start location for FunctionDecl or other ...Calls, or End locattion for Stmt
        SourceLocation location;

        // Locattion for UNSAFE block or Invalid
        SourceLocation unsafeLoc;

        LifeTime(SourceLocation loc = SourceLocation(), const ScopeType c = std::monostate(), SourceLocation unsafe = SourceLocation())
        : location(loc), scope(c), unsafeLoc(unsafe) {
        }

    };

    class LifeTimeScope : SCOPE(protected) std::deque< LifeTime > { // use deque instead of vector as it preserves iterators when resizing

    public:

        enum ModifyMode : uint8_t {
            UNKNOWN = 0,
            BOTH_MODE = 1,
            EDIT_ONLY = 2,
            CONST_ONLY = 3,
        };


        const CompilerInstance &m_CI;

        LifeTimeScope(const CompilerInstance &inst) : m_CI(inst) {
        }

        SourceLocation testUnsafe() {
            auto iter = rbegin();
            while (iter != rend()) {
                if (iter->unsafeLoc.isValid()) {
                    return iter->unsafeLoc;
                }
                iter++;
            }
            return SourceLocation();
        }

        inline bool testInplaceCaller() {

            static_assert(std::is_same_v < std::monostate, std::variant_alternative_t < 0, LifeTime::ScopeType>>);
            static_assert(std::is_same_v < const FunctionDecl *, std::variant_alternative_t < 1, LifeTime::ScopeType>>);

            return back().scope.index() >= 2;
        }

        static ModifyMode testMode(const LifeTime::ScopeType &scope) {
            if (std::holds_alternative<const MemberExpr *>(scope)) {
                const MemberExpr *member = std::get<const MemberExpr *>(scope);

                // llvm::outs() << "testMode: " << member->getMemberDecl()->getNameAsString() << " " << member->getMemberDecl() << "\n";

                DeclContext * cls = member->getMemberDecl()->getDeclContext();
                if (cls) {

                    bool const_count = false;
                    bool non_const_count = false;

                    for (auto elem : cls->noload_lookup(member->getMemberDecl()->getDeclName())) {
                        if (const CXXMethodDecl * method = dyn_cast<CXXMethodDecl>(elem)) {
                            if (method->isConst()) {
                                const_count = true;
                            } else {
                                non_const_count = true;
                            }
                        }
                    }

                    if (const_count && non_const_count) {
                        return ModifyMode::BOTH_MODE;
                    } else if (!const_count && non_const_count) {
                        return ModifyMode::EDIT_ONLY;
                    } else if (const_count && !non_const_count) {
                        return ModifyMode::CONST_ONLY;
                    }
                }
            } else if (std::holds_alternative<const CXXOperatorCallExpr *>(scope)) {

                const CXXOperatorCallExpr *op = std::get<const CXXOperatorCallExpr *>(scope);
                if (op->isAssignmentOp()) {
                    return ModifyMode::EDIT_ONLY;
                } else if (op->isComparisonOp() || op->isInfixBinaryOp()) {
                    return ModifyMode::CONST_ONLY;
                }

            }
            return ModifyMode::UNKNOWN;
        }

        static std::string getName(const LifeTime::ScopeType &scope) {

            static_assert(std::is_same_v <const CallExpr *, std::variant_alternative_t < 4, LifeTime::ScopeType>>);
            static_assert(std::is_same_v <const MemberExpr *, std::variant_alternative_t < 7, LifeTime::ScopeType>>);


            const CallExpr * call = nullptr;
            if (std::holds_alternative<const MemberExpr *>(scope)) {
                //                llvm::outs() << "getMemberNameInfo(): " << std::get<const MemberExpr *>(scope)->getMemberDecl()->getNameAsString() << "\n";
                return std::get<const MemberExpr *>(scope)->getMemberNameInfo().getAsString();

            } else if (std::holds_alternative<const FunctionDecl *>(scope)) {

                return std::get<const FunctionDecl *>(scope)->getNameAsString();

            } else if (std::holds_alternative<const CXXMemberCallExpr *>(scope)) {
                call = std::get<const CXXMemberCallExpr *>(scope);
            } else if (std::holds_alternative<const CXXOperatorCallExpr *>(scope)) {
                call = std::get<const CXXOperatorCallExpr *>(scope);
            } else if (std::holds_alternative<const CallExpr *>(scope)) {
                call = std::get<const CallExpr *>(scope);
            }



            if (call) {
                return call->getDirectCallee()->getQualifiedNameAsString();
            }

            return "";
        }

        const char * findVariable(std::string_view name, int &level) {
            if (name.empty()) {
                return "";
            }
            auto iter = rbegin();
            while (iter != rend()) {
                auto found_var = iter->vars.find(name.begin());
                if (found_var != iter->vars.end()) {
                    level = std::distance(iter, rend());
                    return found_var->second.type;
                }
                auto found_other = iter->other.find(name.begin());
                if (found_other != iter->other.end()) {
                    return "";
                }
                iter++;
            }
            return nullptr;
        }

        std::string getCalleeName() {
            auto iter = rbegin();
            while (iter != rend()) {
                std::string result = getName(iter->scope);
                if (!result.empty()) {
                    return result;
                }
                iter++;
            }
            return "";
        }

        std::string getClassName() {
            auto iter = rbegin();
            while (iter != rend()) {
                if (std::holds_alternative<const CXXRecordDecl *>(iter->scope)) {
                    return std::get<const CXXRecordDecl *>(iter->scope)->getQualifiedNameAsString();
                }
                iter++;
            }
            return "";
        }

        SourceLocation testArgument() {
            auto iter = rbegin();
            while (iter != rend()) {
                if (iter->unsafeLoc.isValid()) {
                    return iter->unsafeLoc;
                }
                iter++;
            }
            return SourceLocation();
        }

        void PushScope(SourceLocation loc, LifeTime::ScopeType call = std::monostate(), SourceLocation unsafe = SourceLocation()) {
            push_back(LifeTime(loc, call, unsafe));
        }

        void PopScope() {
            assert(size() > 1); // First level reserved for static objects
            pop_back();
        }

        LifeTime & back() {
            assert(size()); // First level reserved for static objects
            return std::deque< LifeTime >::back();
        }

        void AddVarDecl(const VarDecl * decl, const char *type, std::string name = "") {
            assert(decl);
            if (name.empty()) {
                name = decl->getNameAsString();
            }
            assert(back().vars.find(name) == back().vars.end());
            back().vars.emplace(name, DeclInfo({decl, type}));
        }

        LifeTime::DependentType::iterator findDependent(const std::string_view name) {
            for (auto level = rbegin(); level != rend(); level++) {
                auto found = level->dependent.find(name.begin());
                if (found != level->dependent.end()) {
                    return found;
                }
            }
            return LifeTime::DependentEnd;
        }

        LifeTime::BlockerType::iterator findBlocker(const std::string_view name) {
            for (auto level = rbegin(); level != rend(); level++) {
                auto found = level->blocker.find(name.begin());
                if (found != level->blocker.end()) {
                    return found;
                }
            }
            return LifeTime::BlockerEnd;
        }

        std::string Dump(const SourceLocation &loc, std::string_view filter) {

            std::string result = MEMSAFE_KEYWORD_START_DUMP;
            if (loc.isValid()) {
                if (loc.isMacroID()) {
                    result += m_CI.getSourceManager().getExpansionLoc(loc).printToString(m_CI.getSourceManager());
                } else {
                    result += loc.printToString(m_CI.getSourceManager());
                }
                result += ": ";
            }
            if (!filter.empty()) {
                //@todo Create Dump filter 
                result += std::format(" filter '{}' not implemented!", filter.begin());
            }
            result += "\n";

            auto iter = begin();
            while (iter != end()) {

                if (iter->location.isValid()) {
                    result += iter->location.printToString(m_CI.getSourceManager());

                    std::string name = getName(iter->scope);
                    if (!name.empty()) {
                        result += " [";
                        result += name;
                        result += "]";
                    }

                } else {
                    result += " #static ";
                }

                result += ": ";

                std::string list;
                auto iter_list = iter->vars.begin();
                while (iter_list != iter->vars.end()) {

                    if (!list.empty()) {
                        list += ", ";
                    }

                    list += iter_list->first;
                    iter_list++;
                }

                result += list;


                std::string dep_str;
                auto dep_list = iter->dependent.begin();
                while (dep_list != iter->dependent.end()) {

                    if (!dep_str.empty()) {
                        dep_str += ", ";
                    }

                    dep_str += "(";
                    dep_str += dep_list->first;
                    dep_str += "=>";
                    dep_str += dep_list->second;
                    dep_str += ")";

                    dep_list++;
                }

                if (!dep_str.empty()) {
                    result += " #dep ";
                    result += dep_str;
                }


                std::string other_str;
                auto other_list = iter->other.begin();
                while (other_list != iter->other.end()) {

                    if (!other_str.empty()) {
                        other_str += ", ";
                    }

                    other_str += *other_list;
                    other_list++;
                }

                if (!other_str.empty()) {
                    result += " #other ";
                    result += other_str;
                }

                result += "\n";
                iter++;
            }
            return result;
        }

    };

    /*
     * 
     * 
     */

    enum LogLevel : uint8_t {
        INFO = 0,
        WARN = 1,
        ERR = 3,
    };

    enum ClassType : uint8_t {
        UNKNOWN = 0,
        AUTO = 1,
        WEAK = 2,
        SHARED = 3,
        NOT_SHARED = 4,
    };

    class MemSafePlugin : public RecursiveASTVisitor<MemSafePlugin> {
    public:
        static inline const char * ERROR_TYPE = MEMSAFE_KEYWORD_ERROR "-type";
        static inline const char * WARNING_TYPE = MEMSAFE_KEYWORD_WARNING "-type";

        static inline const char * AUTO_TYPE = MEMSAFE_KEYWORD_AUTO_TYPE;
        static inline const char * SHARED_TYPE = MEMSAFE_KEYWORD_SHARED_TYPE;
        static inline const char * INVALIDATE_FUNC = MEMSAFE_KEYWORD_INVALIDATE_FUNC;

        static inline const std::set<std::string> attArgs{SHARED_TYPE, AUTO_TYPE, INVALIDATE_FUNC};


        // The first string arguments in the `memsafe` attribute for working and managing the plugin
        static inline const char * PROFILE = MEMSAFE_KEYWORD_PROFILE;
        static inline const char * STATUS = MEMSAFE_KEYWORD_STATUS;
        static inline const char * LEVEL = MEMSAFE_KEYWORD_LEVEL;

        static inline const char * ERROR = MEMSAFE_KEYWORD_ERROR;
        static inline const char * WARNING = MEMSAFE_KEYWORD_WARNING;
        static inline const char * NOTE = MEMSAFE_KEYWORD_NOTE;
        static inline const char * REMARK = MEMSAFE_KEYWORD_REMARK;
        static inline const char * IGNORED = MEMSAFE_KEYWORD_IGNORED;

        static inline const char * BASELINE = MEMSAFE_KEYWORD_BASELINE;
        static inline const char * UNSAFE = MEMSAFE_KEYWORD_UNSAFE;
        static inline const char * PRINT_AST = MEMSAFE_KEYWORD_PRINT_AST;
        static inline const char * PRINT_DUMP = MEMSAFE_KEYWORD_PRINT_DUMP;

        static inline const char * STATUS_ENABLE = MEMSAFE_KEYWORD_ENABLE;
        static inline const char * STATUS_DISABLE = MEMSAFE_KEYWORD_DISABLE;
        static inline const char * STATUS_PUSH = MEMSAFE_KEYWORD_PUSH;
        static inline const char * STATUS_POP = MEMSAFE_KEYWORD_POP;

        std::set<std::string> m_listFirstArg{PROFILE, STATUS, LEVEL, UNSAFE, SHARED_TYPE, AUTO_TYPE, INVALIDATE_FUNC, WARNING_TYPE, ERROR_TYPE, BASELINE, PRINT_AST, PRINT_DUMP};
        std::set<std::string> m_listStatus{STATUS_ENABLE, STATUS_DISABLE, STATUS_PUSH, STATUS_POP};
        std::set<std::string> m_listLevel{ERROR, WARNING, NOTE, REMARK, IGNORED};


        typedef std::map<std::string, std::pair<const CXXRecordDecl *, SourceLocation>> ClassListType;
        typedef std::variant<const CXXRecordDecl *, std::string> LocationType;

        bool m_is_cyclic_analysis;
        MemSafeFile::ClassReadType m_classes;
        ClassListType m_not_shared_class;

        /**
         * List of base classes that contain strong pointers to data 
         * (copying a variable does not copy the data,  only the reference 
         * and increments the ownership counter).
         */
        std::map<std::string, LocationType> m_shared_type;
        /**
         * List of base classes that contain strong pointers to data 
         * that can only be created in temporary variables 
         * (which are automatically deallocated when they go out of scope).
         */
        std::set<std::string> m_auto_type;
        /**
         * List of base iterator types that must be tracked for strict control of dependent pointers.         
         */
        std::set<std::string> m_invalidate_func;

        std::set<std::string> m_warning_type;
        std::set<std::string> m_error_type;

        std::vector<bool> m_status{false};


        clang::DiagnosticsEngine::Level m_level_non_const_arg;
        clang::DiagnosticsEngine::Level m_level_non_const_method;
        clang::DiagnosticsEngine::Level m_diagnostic_level;

        static const inline std::pair<std::string, std::string> pair_empty{std::make_pair<std::string, std::string>("", "")};

        int64_t line_base;
        int64_t line_number;

        LifeTimeScope m_scopes;

        memsafe::StringMatcher m_dump_matcher;
        SourceLocation m_dump_location;
        SourceLocation m_trace_location;

        const CompilerInstance &m_CI;

        MemSafePlugin(const CompilerInstance &instance) :
        m_CI(instance), m_scopes(instance), line_base(0), line_number(0) {
            // Zero level for static variables
            m_scopes.PushScope(SourceLocation(), std::monostate(), SourceLocation());

            m_diagnostic_level = clang::DiagnosticsEngine::Level::Error;

            m_is_cyclic_analysis = true;
        }

        inline clang::DiagnosticsEngine &getDiag() {
            return m_CI.getDiagnostics();
        }

        void clear() {
            //            MemSafePlugin empty(m_CI);
            //            std::swap(*this, empty);
        }

        void dump(raw_ostream & out) {
            out << "\n#memsafe-config\n";
            out << "error-type: " << makeHelperString(m_error_type) << "\n";
            out << "warning-type: " << makeHelperString(m_warning_type) << "\n";
            out << MEMSAFE_KEYWORD_AUTO_TYPE ": " << makeHelperString(m_auto_type) << "\n";
            out << MEMSAFE_KEYWORD_SHARED_TYPE ": " << makeHelperString(m_shared_type) << "\n";
            out << "not-shared-classes: " << makeHelperString(m_not_shared_class) << "\n";
            out << MEMSAFE_KEYWORD_INVALIDATE_FUNC ": " << makeHelperString(m_invalidate_func) << "\n";
            out << "\n";
        }

        clang::DiagnosticsEngine::Level getLevel(clang::DiagnosticsEngine::Level original) {
            SourceLocation loc = m_scopes.testUnsafe();
            if (loc.isValid()) {
                return clang::DiagnosticsEngine::Level::Ignored;
            }
            if (!isEnabledStatus()) {
                return clang::DiagnosticsEngine::Level::Ignored;
            } else if (original > m_diagnostic_level) {
                return m_diagnostic_level;
            }
            return original;
        }

        std::string LogPos(const SourceLocation &loc) {

            size_t line_no = 0;
            if (loc.isMacroID()) {
                line_no = m_CI.getSourceManager().getSpellingLineNumber(m_CI.getSourceManager().getExpansionLoc(loc));
            } else {
                line_no = m_CI.getSourceManager().getSpellingLineNumber(loc);
            }

            if (loc.isValid()) {
                return std::format("{}", line_no - line_base + line_number);
            }
            return "0";
        }

        void LogOnly(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation(), LogLevel level = LogLevel::INFO) {
            if (logger) {
                const char * prefix = nullptr;
                switch (level) {
                    case LogLevel::INFO:
                        prefix = "log";
                        break;
                    case LogLevel::WARN:
                        prefix = "warn";
                        break;
                    case LogLevel::ERR:
                        prefix = "err";
                        break;
                }
                logger->Log(loc, std::format("#{} #{} {}", prefix, hash.isValid() ? LogPos(hash) : LogPos(loc), str));
            }
        }

        void LogWarning(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
            getDiag().Report(loc, getDiag().getCustomDiagID(
                    getLevel(clang::DiagnosticsEngine::Warning), "%0"))
                    .AddString(str);
            LogOnly(loc, str, hash, LogLevel::WARN);
        }

        void LogError(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
            getDiag().Report(loc, getDiag().getCustomDiagID(
                    getLevel(clang::DiagnosticsEngine::Error), "%0"))
                    .AddString(str);
            LogOnly(loc, str, hash, LogLevel::ERR);
        }

        template <typename T>
        static std::string makeHelperString(const T &map) {
            std::string result;
            for (auto elem : map) {
                if (!result.empty()) {
                    result += "', '";
                }
                result += elem.first;
            }
            result.insert(0, "'");
            result += "'";
            return result;
        }

        static std::string makeHelperString(const std::set<std::string> &set) {
            std::string result;
            for (auto &elem : set) {

                if (!result.empty()) {
                    result += "', '";
                }
                result += elem;
            }
            result.insert(0, "'");
            result += "'";
            return result;
        }

        static bool checkBehavior(const std::string_view str, clang::DiagnosticsEngine::Level *level = nullptr) {
            if (str.compare(ERROR) == 0) {
                if (level) {
                    *level = clang::DiagnosticsEngine::Level::Error;
                }
                return true;
            } else if (str.compare(WARNING) == 0) {
                if (level) {
                    *level = clang::DiagnosticsEngine::Level::Warning;
                }
                return true;
            } else if (str.compare(IGNORED) == 0) {
                if (level) {
                    *level = clang::DiagnosticsEngine::Level::Ignored;
                }
                return true;
            }
            return false;
        }

        static std::string unknownArgumentHelper(const std::string_view arg, const std::set<std::string> & set) {
            std::string result = "Unknown argument '";
            result += arg.begin();
            result += "'. Expected string argument from the following list: ";
            result += makeHelperString(set);
            return result;
        }

        std::string processArgs(std::string_view first, std::string_view second, SourceLocation loc) {

            std::string result;
            if (first.empty() || second.empty()) {
                if (first.compare(PROFILE) == 0) {
                    clear();
                } else if (first.compare(UNSAFE) == 0 || first.compare(PRINT_AST) == 0 || first.compare(PRINT_DUMP) == 0) {
                    // The second argument may be empty
                } else {
                    result = "Two string literal arguments expected!";
                }
            } else if (m_listFirstArg.find(first.begin()) == m_listFirstArg.end()) {
                result = unknownArgumentHelper(first.begin(), m_listFirstArg);
            }

            static const char * LEVEL_ERROR_MESSAGE = "Required behavior not recognized! Allowed values: '"
                    MEMSAFE_KEYWORD_ERROR "', '" "', '" MEMSAFE_KEYWORD_WARNING "' or '" MEMSAFE_KEYWORD_IGNORED "'.";

            if (result.empty()) {
                if (first.compare(STATUS) == 0) {
                    if (m_listStatus.find(second.begin()) == m_listStatus.end()) {
                        result = unknownArgumentHelper(second, m_listStatus);
                    } else {
                        if (m_status.empty()) {
                            result = "Violation of the logic of saving and restoring the state of the plugin";
                        } else {
                            if (second.compare(STATUS_ENABLE) == 0) {
                                *m_status.rbegin() = true;
                            } else if (second.compare(STATUS_DISABLE) == 0) {
                                *m_status.rbegin() = false;
                            } else if (second.compare(STATUS_PUSH) == 0) {
                                m_status.push_back(true);
                            } else if (second.compare(STATUS_POP) == 0) {
                                if (m_status.size() == 1) {
                                    result = "Violation of the logic of saving and restoring the state of the plugin";
                                } else {
                                    m_status.pop_back();
                                }
                            }
                        }
                    }
                } else if (first.compare(LEVEL) == 0) {

                    if (m_listLevel.find(second.begin()) == m_listLevel.end()) {
                        result = unknownArgumentHelper(second, m_listLevel);
                    } else if (!checkBehavior(second, &m_diagnostic_level)) {
                        result = LEVEL_ERROR_MESSAGE;
                    }

                } else if (first.compare(PROFILE) == 0) {
                    if (!second.empty()) {
                        result = "Loading profile from file is not implemented!";
                    }
                } else if (first.compare(ERROR_TYPE) == 0) {
                    m_error_type.emplace(second.begin());
                } else if (first.compare(WARNING_TYPE) == 0) {
                    m_warning_type.emplace(second.begin());
                } else if (first.compare(SHARED_TYPE) == 0) {
                    m_shared_type.emplace(second.begin(), MemSafeLogger::LocToStr(loc, m_CI.getSourceManager()));
                } else if (first.compare(AUTO_TYPE) == 0) {
                    m_auto_type.emplace(second.begin());
                } else if (first.compare(INVALIDATE_FUNC) == 0) {
                    m_invalidate_func.emplace(second.begin());
                }
            }
            return result;
        }

        /**
         * Find class type by name
         */

        const char * findClassType(std::string_view type, bool local_only = false) {

            if (m_auto_type.find(type.begin()) != m_auto_type.end()) {
                return AUTO_TYPE;
            }
            std::map<std::string, LocationType>::iterator found;
            found = m_shared_type.find(type.begin());
            if (found != m_shared_type.end()) {
                return (!local_only || std::holds_alternative<const CXXRecordDecl *>(found->second)) ? SHARED_TYPE : nullptr;
            }
            return nullptr;
        }

        std::string LocationTypeToString(LocationType & loc) {
            if (std::holds_alternative<std::string>(loc)) {
                return std::get<std::string>(loc);
            }
            assert(std::holds_alternative<const CXXRecordDecl *>(loc));
            return MemSafeLogger::LocToStr(std::get<const CXXRecordDecl *>(loc)->getLocation(), m_CI.getSourceManager());
        }

        void setCyclicAnalysis(bool status) {
            m_is_cyclic_analysis = status;
        }

        /* 
         * A reference class is a class that has at least one field, 
         * or at least one field of any parent class, 
         * or the parent class itself has a reference type 
         * (i.e. the shared-type list contains the name of the class or one of its parents)
         * or has a pointer (reference) to a structured data type.
         * 
         * All parent classes must have a definition in the current translation unit 
         * (a forward definition is not sufficient, or a compile-time error will occur).
         * 
         * This means that the definitions of all its parent classes and 
         * the types of fields must be available when the class is defined. 
         * However, fields with reference types may have a forward type declaration 
         * (i.e. not have a definition in the current translation unit).
         * 
         * class Pred;
         * class Class {
         *      std::shared_ptr<Pred> pred; // This reference type and definition of class Pred is not required
         * };
         * 
         * Therefore, to check whether a struct is a reference class 
         * (whether the class contains reference data types), 
         * the definition of reference data types in the class fields is not required.
         */

        bool testSharedType(const CXXRecordDecl &decl, ClassListType &list) {

            bool is_shared = false;
            auto found_shared = m_shared_type.find(decl.getQualifiedNameAsString());
            if (found_shared != m_shared_type.end()) {

                LogOnly(decl.getLocation(), std::format("Detected shared type '{}' registered at {}",
                        decl.getQualifiedNameAsString(), LocationTypeToString(found_shared->second)));

                is_shared = true;

            } else {

                for (auto elem : list) {
                    if (m_shared_type.find(elem.first) != m_shared_type.end()) {

                        is_shared = true;

                        // At least one parent or field type of the class is a reference class, 
                        // which means that the class itself is also a reference class.
                        m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                        LogOnly(decl.getLocation(), std::format("Add shared type '{}'", decl.getQualifiedNameAsString()));
                        break;
                    }
                }
            }
            return is_shared;
        }

        /** 
         * Make the list of used classes by recursively traversing parent classes and all their field types.
         */
        void MakeUsedClasses(const CXXRecordDecl &decl, ClassListType &used, ClassListType &fields) {
            if (used.find(decl.getQualifiedNameAsString()) != used.end()) {
                return;
            }

            if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl> (&decl)) {

                if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
                    m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                } else {
                    MakeUsedClasses(*Special->getSpecializedTemplate()->getTemplatedDecl(), used, fields);
                }

                const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
                const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

                int Index = 0;
                for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
                    if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
                        if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
                            if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
                                m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                            }
                            used[type->getQualifiedNameAsString()] = {type, Special->getLocation()};
                            MakeUsedClasses(*type, used, fields);
                        }
                    }
                    Index++;
                }
            }

            if (decl.hasDefinition()) {

                for (auto iter = decl.bases_begin(); iter != decl.bases_end(); iter++) {

                    // iter->getType()->dump();

                    if (const ElaboratedType * templ = dyn_cast<const ElaboratedType> (iter->getType())) {

                        if (const TemplateSpecializationType * Special = dyn_cast<const TemplateSpecializationType> (templ->desugar())) {
                            if (const ClassTemplateDecl * templ_class = dyn_cast<const ClassTemplateDecl> (Special->getTemplateName().getAsTemplateDecl())) {
                                assert(templ_class->getTemplatedDecl());

                                if (m_shared_type.find(templ_class->getTemplatedDecl()->getQualifiedNameAsString()) != m_shared_type.end()) {
                                    m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                                } else {
                                    //@todo Do you need all the descendants or can you limit the list to the first template class?
                                    used[templ_class->getTemplatedDecl()->getQualifiedNameAsString()] = {templ_class->getTemplatedDecl(), iter->getBeginLoc()};
                                    MakeUsedClasses(*templ_class->getTemplatedDecl(), used, fields);
                                }

                                for (auto elem : Special->template_arguments()) {
                                    //                                    llvm::outs() << elem.getKind() << "\n";
                                    if (elem.getKind() == TemplateArgument::ArgKind::Type) {
                                        if (const CXXRecordDecl * type = elem.getAsType()->getAsCXXRecordDecl()) {
                                            if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
                                                m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                                            } else {
                                                //@todo Do you need all the descendants or can you limit the list to the first template class?
                                                used[type->getQualifiedNameAsString()] = {type, iter->getBeginLoc()};
                                                MakeUsedClasses(*elem.getAsType()->getAsCXXRecordDecl(), used, fields);
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (const RecordType * rec = dyn_cast<const RecordType> (templ->desugar())) {
                            if (const CXXRecordDecl * base = rec->desugar()->getAsCXXRecordDecl()) {
                                if (m_shared_type.find(base->getQualifiedNameAsString()) != m_shared_type.end()) {
                                    m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                                }
                                used[base->getQualifiedNameAsString()] = {base, iter->getBeginLoc()};
                                MakeUsedClasses(*base, used, fields);
                            }
                        }

                    } else if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl> (iter->getType()->getAsCXXRecordDecl())) {

                        if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
                            m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                        } else {
                            MakeUsedClasses(*Special->getSpecializedTemplate()->getTemplatedDecl(), used, fields);
                        }

                        const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
                        const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

                        int Index = 0;
                        for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
                            if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
                                if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
                                    if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
                                        m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                                    }
                                    used[type->getQualifiedNameAsString()] = {type, iter->getBeginLoc()};
                                    MakeUsedClasses(*type, used, fields);
                                }
                            }
                            Index++;
                        }

                    } else if (const CXXRecordDecl * base = iter->getType()->getAsCXXRecordDecl()) {

                        if (m_shared_type.find(base->getQualifiedNameAsString()) != m_shared_type.end()) {
                            m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                        }
                        used[base->getQualifiedNameAsString()] = {base, iter->getBeginLoc()};
                        MakeUsedClasses(*base, used, fields);
                    }
                }

                for (auto field = decl.field_begin(); field != decl.field_end(); field++) {

                    if (field->getCanonicalDecl()->getType()->isPointerOrReferenceType()) {

                        QualType CanonicalType = field->getCanonicalDecl()->getType()->getPointeeType();
                        //  llvm::outs() << "isPointerOrReferenceType " << CanonicalType.getAsString() << "\n";

                        if (const auto *RT = CanonicalType->getAs<RecordType>()) {
                            if (const auto *ClassDecl = dyn_cast<CXXRecordDecl>(RT->getDecl())) {

                                // the presence of a field with a reference to a structured type 
                                // means that the current type is a reference data type
                                m_shared_type[decl.getQualifiedNameAsString()] = &decl;


                                if (fields.find(ClassDecl->getQualifiedNameAsString()) == fields.end() && fields.find(decl.getQualifiedNameAsString()) == fields.end()) {

                                    fields[ClassDecl->getQualifiedNameAsString()] = {ClassDecl, field->getLocation()};

                                    LogOnly(field->getLocation(), std::format("Field with reference to structured data type '{}'", ClassDecl->getQualifiedNameAsString()));

                                    // Save the type name in the list of used data types 
                                    // for future analysis of circular references.
                                    MakeUsedClasses(*ClassDecl, used, fields);
                                }

                            }
                        }
                    }


                    const CXXRecordDecl * field_type = field->getType()->getAsCXXRecordDecl();
                    if (!field_type) {
                        continue;
                    }


                    if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl> (field_type)) {

                        if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
                            m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                        } else {
                            if (fields.find(Special->getQualifiedNameAsString()) == fields.end() && fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
                                MakeUsedClasses(*Special, used, fields);
                            }
                        }

                        // std::string diag_name;
                        // llvm::raw_string_ostream str_out(diag_name);
                        // Special->getNameForDiagnostic(str_out,m_CI.getASTContext().getPrintingPolicy(), true);
                        // llvm::outs() << diag_name << "\n";

                        const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
                        const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

                        int Index = 0;
                        for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
                            if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
                                if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
                                    if (fields.find(type->getQualifiedNameAsString()) == fields.end() && fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
                                        if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
                                            m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                                        }
                                        fields[type->getQualifiedNameAsString()] = {type, field->getLocation()};
                                        MakeUsedClasses(*type, used, fields);
                                    }
                                }
                            }
                            Index++;
                        }

                    } else {
                        if (fields.find(field_type->getQualifiedNameAsString()) == fields.end() && fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
                            if (m_shared_type.find(field_type->getQualifiedNameAsString()) != m_shared_type.end()) {
                                m_shared_type[decl.getQualifiedNameAsString()] = &decl;
                            }
                            fields[field_type->getQualifiedNameAsString()] = {field_type, field->getLocation()};
                            MakeUsedClasses(*field_type, used, fields);
                        }
                    }
                }
            }
        }

        void reduceSharedList(ClassListType & list, bool test_external = true) {
            auto iter = list.begin();
            while (iter != list.end()) {
                if (iter->second.first->hasDefinition()) {

                    auto found_shared = m_shared_type.find(iter->first);
                    if (found_shared == m_shared_type.end()) {
                        iter = list.erase(iter);
                    } else {
                        iter++;
                    }

                } else if (test_external) {

                    auto found = m_classes.find(iter->first);
                    if (found == m_classes.end()) {

                        std::string message = std::format("Class definition '{}' not found in current translation unit.", iter->first);

                        LogError(iter->second.second, message);

                        getDiag().Report(iter->second.second,
                                getDiag().getCustomDiagID(clang::DiagnosticsEngine::Error, ""
                                "%0\n"
                                "The circular reference analyzer requires two passes.\n"
                                "First run the plugin with key '--circleref-write -fsyntax-only' to generate the class list,\n"
                                "then run a second time with the '--circleref-read' key to re-analyze,\n"
                                "or disable the circular reference analyzer with the 'circleref-disable' option.\n"))
                                .AddString(message);

                        throw std::runtime_error(message);

                    } else {
                        if (found->second.parents.empty() && found->second.fields.empty()) {
                            LogOnly(iter->second.second, std::format("Non shared class definition '{}' used from another translation unit.", iter->first));
                            iter = list.erase(iter);
                        } else {
                            LogOnly(iter->second.second, std::format("Shared class definition '{}' used from another translation unit.", iter->first));
                            iter++;
                        }
                    }
                } else {
                    iter++;
                }
            }
        }

        /**
         * A class with cyclic relationships that has a field of reference type to its own type,
         * or its field has a reference type to another class that has a field of reference type
         * to the class being checked, either directly or through reference fields of other classes.
         */
        bool checkCycles(const CXXRecordDecl &decl, ClassListType & used, ClassListType & fields) {

            // Remove all not shared classes
            reduceSharedList(fields);

            used[decl.getQualifiedNameAsString()] = {&decl, decl.getLocation()};

            for (auto parent : used) {
                auto field_found = fields.find(parent.first);
                if (field_found != fields.end()) {
                    LogError(field_found->second.second, std::format("Class {} has a reference to itself through the field type {}",
                            decl.getQualifiedNameAsString(), field_found->first));
                    return false;
                }
            }

            ClassListType other;
            ClassListType other_fields;
            for (auto elem : fields) {

                assert(elem.second.first);

                other.clear();
                other_fields.clear();

                MakeUsedClasses(*elem.second.first, other, other_fields);

                reduceSharedList(other_fields);

                auto other_found = other_fields.find(elem.first);
                if (other_found != other_fields.end()) {
                    bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(decl) || checkDeclUnsafe(*elem.second.first) || checkDeclUnsafe(*other_found->second.first);

                    if (is_unsafe) {
                        LogWarning(other_found->second.second, std::format("UNSAFE The class '{}' has a circular reference through class '{}'",
                                decl.getQualifiedNameAsString(), other_found->first));
                    } else {
                        // Class {} has a cross reference through a field with type вввввв
                        LogError(other_found->second.second, std::format("The class '{}' has a circular reference through class '{}'",
                                decl.getQualifiedNameAsString(), other_found->first));
                    }
                    return false;
                }

                for (auto elem_other : other_fields) {
                    if (used.count(elem_other.first)) {

                        bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(decl) || checkDeclUnsafe(*elem.second.first) || checkDeclUnsafe(*elem_other.second.first);

                        if (is_unsafe) {
                            LogWarning(elem_other.second.second, std::format("UNSAFE The class '{}' has a circular reference through class '{}'",
                                    elem.first, decl.getQualifiedNameAsString()));
                        } else {
                            LogError(elem_other.second.second, std::format("The class '{}' has a circular reference through class '{}'",
                                    elem.first, decl.getQualifiedNameAsString()));
                        }
                        return false;
                    }
                }
            }

            return true;
        }

        inline bool isEnabledStatus() {
            assert(!m_status.empty());
            return *m_status.rbegin();
        }

        /*
         * plugin helper methods
         * 
         */
        inline bool isEnabled() {
            return isEnabledStatus() && !scaner;
        }

        SourceLocation checkUnsafeBlock(const AttributedStmt * attrStmt) {

            if (attrStmt) {

                auto attrs = attrStmt->getAttrs();

                for (auto &elem : attrs) {

                    std::pair<std::string, std::string> pair = parseAttr(dyn_cast_or_null<AnnotateAttr>(elem));
                    if (pair != pair_empty) {

                        if (pair.first.compare(UNSAFE) == 0) {

                            if (logger) {
                                logger->AttrComplete(elem->getLocation());
                            }

                            LogOnly(attrStmt->getBeginLoc(), "Unsafe statement", attrStmt->getBeginLoc());

                            return elem->getLocation();
                        }
                    }

                }
            }
            return SourceLocation();
        }

        std::pair<std::string, std::string> parseAttr(const AnnotateAttr * const attr) {
            if (!attr || attr->getAnnotation() != TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE) || attr->args_size() != 2) {
                return pair_empty;
            }

            clang::AnnotateAttr::args_iterator result = attr->args_begin();
            StringLiteral * first = dyn_cast_or_null<StringLiteral>(*result);
            result++;
            StringLiteral * second = dyn_cast_or_null<StringLiteral>(*result);

            if (!first || !second) {
                getDiag().Report(attr->getLocation(), getDiag().getCustomDiagID(
                        DiagnosticsEngine::Error, "Two string literal arguments expected!"));
                return pair_empty;
            }
            return std::make_pair < std::string, std::string>(first->getString().str(), second->getString().str());
        }

        /*
         * 
         * 
         * 
         */

        bool checkDeclUnsafe(const Decl &decl) {
            auto attr_args = parseAttr(decl.getAttr<AnnotateAttr>());
            if (attr_args != pair_empty) {
                return attr_args.first.compare(UNSAFE) == 0;
            }
            return false;
        }

        void checkDeclAttributes(const Decl *decl) {
            // Check namespace annotation attribute
            // This check should be done first as it is used to enable and disable the plugin.

            if (!decl) {
                return;
            }

            AnnotateAttr * attr = decl->getAttr<AnnotateAttr>();
            auto attr_args = parseAttr(attr);

            if (attr_args != pair_empty) {

                std::string error_str = processArgs(attr_args.first, attr_args.second, decl->getLocation());

                if (!error_str.empty()) {

                    clang::DiagnosticBuilder DB = getDiag().Report(attr->getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Error, "Error detected: %0"));
                    DB.AddString(error_str);

                    LogError(attr->getLocation(), error_str);

                } else if (attr_args.first.compare(BASELINE) == 0) {

                    SourceLocation loc = decl->getLocation();
                    if (loc.isMacroID()) {
                        loc = m_CI.getSourceManager().getExpansionLoc(loc);
                    }

                    try {
                        int old_line_number = line_number;
                        line_number = std::stoi(SeparatorRemove(attr_args.second));
                        line_base = getDiag().getSourceManager().getSpellingLineNumber(loc);

                        if (old_line_number >= line_number) {
                            LogOnly(loc, "Error in base sequential numbering");
                            getDiag().Report(loc, getDiag().getCustomDiagID(DiagnosticsEngine::Error, "Error in base sequential numbering"));
                        }

                    } catch (...) {
                        LogOnly(loc, "The second argument is expected to be a line number as a literal string!");
                        getDiag().Report(loc, getDiag().getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "The second argument is expected to be a line number as a literal string!"));
                    }

                } else if (attr_args.first.compare(STATUS) == 0) {

                    clang::DiagnosticBuilder DB = getDiag().Report(decl->getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Note,
                            "Status memory safety plugin is %0!"));
                    DB.AddString(attr_args.second);

                }

                if (logger) {
                    logger->AttrComplete(attr->getLocation());
                }

            }
        }

        /*
         * @ref MEMSAFE_DUMP
         */
        void checkDumpFilter(const Decl * decl) {
            if (decl) {
                AnnotateAttr *attr = decl->getAttr<AnnotateAttr>();
                auto attr_args = parseAttr(attr);
                if (attr_args != pair_empty) {
                    if (attr_args.first.compare(PRINT_AST) == 0) {
                        if (attr_args.second.empty()) {
                            m_dump_matcher.Clear();
                        } else {
                            //@todo Create Dump filter 
                            llvm::outs() << "Dump filter '" << attr_args.second << "' not implemented!\n";
                            m_dump_matcher.Create(attr_args.second, ';');
                        }
                        if (decl->getLocation().isMacroID()) {
                            m_dump_location = m_CI.getSourceManager().getExpansionLoc(decl->getLocation());
                        } else {
                            m_dump_location = decl->getLocation();
                        }

                        if (logger) {
                            logger->AttrComplete(attr->getLocation());
                        }

                    } else if (attr_args.first.compare(PRINT_DUMP) == 0) {

                        if (skipLocation(m_trace_location, decl->getLocation())) {
                            return;
                        }

                        if (logger) {
                            logger->AttrComplete(attr->getLocation());
                        }

                        llvm::outs() << m_scopes.Dump(attr->getLocation(), attr_args.second);
                    }
                }
            }
        }

        bool skipLocation(SourceLocation &last, const SourceLocation &loc) {
            int64_t line_no = 0;
            SourceLocation test_loc = loc;

            if (loc.isMacroID()) {
                test_loc = m_CI.getSourceManager().getExpansionLoc(loc);
            }
            if (last.isValid() &&
                    m_CI.getSourceManager().getSpellingLineNumber(test_loc) == m_CI.getSourceManager().getSpellingLineNumber(last)) {
                return true;
            }
            last = test_loc;
            return false;
        }

        void printDumpIfEnabled(const Decl * decl) {
            if (!decl || !isEnabledStatus() || m_dump_matcher.isEmpty() || skipLocation(m_dump_location, decl->getLocation())) {
                return;
            }

            // Source location for IDE
            llvm::outs() << decl->getLocation().printToString(m_CI.getSourceManager());
            // Color highlighting
            llvm::outs() << "  \033[1;46;34m";
            // The string at the current position to expand the AST

            PrintingPolicy Policy(m_CI.getASTContext().getPrintingPolicy());
            Policy.SuppressScope = false;
            Policy.AnonymousTagLocations = true;

            //@todo Create Dump filter 
            std::string output;
            llvm::raw_string_ostream str(output);
            decl->print(str, Policy);

            size_t pos = output.find("\n");
            if (pos == std::string::npos) {
                llvm::outs() << output;
            } else {
                llvm::outs() << output.substr(0, pos - 1);
            }

            // Close color highlighting
            llvm::outs() << "\033[0m ";
            llvm::outs() << " dump:\n";


            // Ast tree for current line
            const ASTContext &Ctx = m_CI.getASTContext();
            ASTDumper P(llvm::outs(), Ctx, /*ShowColors=*/true);
            P.Visit(decl); //dyn_cast<Decl>(decl)

        }

        void printDumpIfEnabled(const Stmt * stmt) {
            if (!stmt || !isEnabledStatus() || m_dump_matcher.isEmpty() || skipLocation(m_dump_location, stmt->getBeginLoc())) {
                return;
            }

            // Source location for IDE
            llvm::outs() << stmt->getBeginLoc().printToString(m_CI.getSourceManager());
            // Color highlighting
            llvm::outs() << "  \033[1;46;34m";
            // The string at the current position to expand the AST

            PrintingPolicy Policy(m_CI.getASTContext().getPrintingPolicy());
            Policy.SuppressScope = false;
            Policy.AnonymousTagLocations = true;

            //@todo Create Dump filter 
            std::string output;
            llvm::raw_string_ostream str(output);
            stmt->printPretty(str, nullptr, Policy);

            size_t pos = output.find("\n");
            if (pos == std::string::npos) {
                llvm::outs() << output;
            } else {
                llvm::outs() << output.substr(0, pos - 1);
            }

            // Close color highlighting
            llvm::outs() << "\033[0m ";
            llvm::outs() << " dump:\n";


            // Ast tree for current line
            const ASTContext &Ctx = m_CI.getASTContext();
            ASTDumper P(llvm::outs(), Ctx, /*ShowColors=*/true);
            P.Visit(stmt); //dyn_cast<Decl>(decl)
        }

        /*
         * 
         * 
         */

        const char * checkClassNameTracking(const CXXRecordDecl * type) {
            const char * result = type ? findClassType(type->getQualifiedNameAsString()) : nullptr;
            const CXXRecordDecl * cxx = dyn_cast_or_null<CXXRecordDecl>(type);
            if (cxx) {
                for (auto iter = cxx->bases_begin(); !result && iter != cxx->bases_end(); iter++) {
                    if (iter->isBaseOfClass()) {
                        if (const CXXRecordDecl * base = iter->getType()->getAsCXXRecordDecl()) {
                            result = checkClassNameTracking(base);
                        }
                    }
                }
            }
            return result;
        }

        const DeclRefExpr * getInitializer(const VarDecl &decl) {
            if (const CXXMemberCallExpr * calle = dyn_cast_or_null<CXXMemberCallExpr>(getExprInitializer(decl))) {
                return dyn_cast<DeclRefExpr>(removeTempExpr(calle->getImplicitObjectArgument()));
            }
            if (const CXXOperatorCallExpr * op = dyn_cast_or_null<CXXOperatorCallExpr>(getExprInitializer(decl))) {
                return dyn_cast<DeclRefExpr>(removeTempExpr(op->getArg(0)));
            }
            return nullptr;
        }

        const Expr * removeTempExpr(const Expr * expr) {
            if (expr && (isa<ImplicitCastExpr>(expr) || isa<ExprWithCleanups>(expr) || isa<CXXBindTemporaryExpr>(expr) || isa<MaterializeTemporaryExpr>(expr))) {
                return expr->IgnoreUnlessSpelledInSource();
            }
            return expr;
        }

        const Expr * getExprInitializer(const VarDecl & var) {

            const Expr * result = removeTempExpr(var.getAnyInitializer());

            if (!result || isa<StringLiteral>(result) || isa<IntegerLiteral>(result)) {
                return nullptr;
            }

            if (isa<DeclRefExpr>(result) || isa<CXXMemberCallExpr>(result) || isa<CXXOperatorCallExpr>(result) || isa<CXXConstructExpr>(result) || isa<UnaryOperator>(result)) {
                return result;
            }

            if (const ParenExpr * paren = dyn_cast<ParenExpr>(result)) {
                return paren->getSubExpr();
            }

            LogError(var.getLocation(), "Unknown VarDecl initializer");
            var.getAnyInitializer()->dumpColor();

            return nullptr;

        }

        std::string getArgName(const Expr * arg) {
            const DeclRefExpr * dre = nullptr;
            if (const Expr * init = removeTempExpr(arg)) {
                dre = dyn_cast<DeclRefExpr>(init->IgnoreUnlessSpelledInSource()->IgnoreImplicit());
            }
            if (dre && dre->getDecl()) {
                return dre->getDecl()->getNameAsString();
            }
            return "";
        }

        void checkTypeName(const SourceLocation &loc, std::string_view name, bool unsafe) {
            if (m_error_type.find(name.begin()) != m_error_type.end()) {
                if (unsafe) {
                    LogWarning(loc, std::format("UNSAFE Error type found '{}'", name));
                } else {
                    LogError(loc, std::format("Error type found '{}'", name));
                }
            } else if (m_warning_type.find(name.begin()) != m_warning_type.end()) {
                if (unsafe) {
                    LogWarning(loc, std::format("UNSAFE Warning type found '{}'", name));
                } else {
                    LogWarning(loc, std::format("Warning type found '{}'", name));
                }
            }
        }

        bool checkArg(const Expr * arg, std::string &name, const char * &type, int &level) {
            name = getArgName(arg);
            if (name.empty()) {
                LogOnly(arg->getBeginLoc(), "Argument is not a variable");
                return false;
            }

            type = m_scopes.findVariable(name, level);
            if (type == nullptr) {
                LogError(arg->getBeginLoc(), "Variable name not found!");
                return false;
            }
            return true;
        }

        /*
         * 
         * RecursiveASTVisitor template methods Visit... for analyzing source code based on the created context
         * 
         * 
         */

        bool VisitDeclRefExpr(const DeclRefExpr * ref) {
            if (isEnabled() && !ref->getDecl()->isFunctionOrFunctionTemplate()) {

                std::string ref_name = ref->getDecl()->getNameAsString();
                std::string caller = LifeTimeScope::getName(m_scopes.back().scope);

                SourceLocation cur_location = ref->getLocation();

                clang::Expr::Classification cls = ref->ClassifyModifiable(m_CI.getASTContext(), cur_location);
                //                Expr::isModifiableLvalueResult mod = ref->isModifiableLvalue(m_CI.getSourceManager(), ref->getLocation());
                //                llvm::outs() << "isModifiable(): " << cls.getModifiable() << "\n";


                auto found = m_scopes.findBlocker(ref_name);
                if (found != LifeTime::BlockerEnd) {
                    if (!found->second.empty() && found->second.front() == cur_location) {
                        // First use -> clear source location
                        found->second.clear();

                    } else {

                        LifeTimeScope::ModifyMode mode = LifeTimeScope::testMode(m_scopes.back().scope);

                        if (mode == LifeTimeScope::ModifyMode::CONST_ONLY) {

                            LogOnly(cur_location, std::format("Only constant method '{}' does not change data.", caller));
                            return true;

                        } else if (mode == LifeTimeScope::ModifyMode::EDIT_ONLY) {

                            LogOnly(cur_location, std::format("Only non constant method '{}' alway changed data.", caller));
                            found->second.push_back(cur_location);

                        } else if (mode == LifeTimeScope::ModifyMode::BOTH_MODE) {

                            if (m_level_non_const_method < clang::DiagnosticsEngine::Level::Warning) {
                                LogOnly(cur_location, std::format("Both methods '{}' for constant and non-constant objects tracking disabled!", caller));
                                return true;
                            } else if (m_level_non_const_method == clang::DiagnosticsEngine::Level::Warning) {
                                LogOnly(cur_location, std::format("Both methods '{}' for constant and non-constant objects warning only!", caller));
                                return true;
                            } else if (m_level_non_const_method > clang::DiagnosticsEngine::Level::Warning) {
                                LogOnly(cur_location, std::format("Both methods '{}' for constant and non-constant objects tracking enabled!", caller));
                                found->second.push_back(cur_location);
                                return true;
                            }
                        }
                    }
                }

                auto depend_found = m_scopes.findDependent(ref_name);
                if (depend_found != LifeTime::DependentEnd) {


                    std::string caller = LifeTimeScope::getName(m_scopes.back().scope);
                    LifeTimeScope::ModifyMode mode = LifeTimeScope::testMode(m_scopes.back().scope);

                    auto block_found = m_scopes.findBlocker(depend_found->second);
                    if (block_found != LifeTime::BlockerEnd) {

                        if (!caller.empty()) {
                            if (m_invalidate_func.find(caller) != m_invalidate_func.end()) {
                                LogOnly(ref->getLocation(), std::format("Call {} '{}'", MEMSAFE_KEYWORD_INVALIDATE_FUNC, caller));
                            } else {
                                caller.clear();
                            }
                        }

                        if (block_found->second.empty()) {
                            LogOnly(ref->getLocation(), std::format("Depended {} corrected!", ref_name));
                        } else {

                            for (auto &elem : block_found->second) {
                                LogWarning(elem, std::format("using main variable '{}'", block_found->first));
                            }
                            LogError(ref->getLocation(), std::format("Using the dependent variable '{}' after changing the main variable '{}'!", ref_name, block_found->first));
                        }
                    }
                }

            }
            return true;
        }

        //  **************************************************************************************************
        //  **************************************************************************************************
        //  **************************************************************************************************
        //  Disabled checks lifetime, ownership and borrowing due to change in approach to reference analysis
        //
        //  **************************************************************************************************
        //  **************************************************************************************************
        //  **************************************************************************************************
        //
        //        bool VisitCallExpr(const CallExpr * call) {
        //            if (isEnabled() && call->getNumArgs() == 2) {
        //
        //                if (call->getDirectCallee()-> getQualifiedNameAsString().compare("std::swap") != 0) {
        //                    // check only swap funstion or method
        //                    return true;
        //                }
        //
        //                std::string lval;
        //                const char * lval_type;
        //                int lval_level;
        //
        //                std::string rval;
        //                const char * rval_type;
        //                int rval_level;
        //
        //                if (checkArg(call->getArg(0), lval, lval_type, lval_level) && checkArg(call->getArg(1), rval, rval_type, rval_level)) {
        //                    if (std::string_view(SHARED_TYPE).compare(lval_type) == 0 && std::string_view(SHARED_TYPE).compare(rval_type) == 0) {
        //                        if (lval_level == rval_level) {
        //                            LogOnly(call->getBeginLoc(), "Swap shared variables with the same lifetime");
        //                        } else {
        //                            if (m_scopes.testUnsafe().isValid()) {
        //                                LogWarning(call->getBeginLoc(), "UNSAFE swap the shared variables with different lifetimes");
        //                            } else {
        //                                LogError(call->getBeginLoc(), "Error swap the shared variables with different lifetimes");
        //                            }
        //                        }
        //                    }
        //                }
        //            }
        //            return true;
        //        }
        //
        //        bool VisitCXXOperatorCallExpr(const CXXOperatorCallExpr * op) {
        //            if (isEnabled() && op->isAssignmentOp()) {
        //
        //                assert(op->getNumArgs() == 2);
        //
        //                std::string lval;
        //                const char * lval_type;
        //                int lval_level;
        //
        //                std::string rval;
        //                const char * rval_type;
        //                int rval_level;
        //
        //                if (checkArg(op->getArg(0), lval, lval_type, lval_level) && checkArg(op->getArg(1), rval, rval_type, rval_level)) {
        //                    if (std::string_view(SHARED_TYPE).compare(lval_type) == 0 && std::string_view(SHARED_TYPE).compare(rval_type) == 0) {
        //                        if (lval_level > rval_level) {
        //                            LogOnly(op->getBeginLoc(), "Copy of shared variable with shorter lifetime");
        //                        } else {
        //                            if (m_scopes.testUnsafe().isValid()) {
        //                                LogWarning(op->getBeginLoc(), "UNSAFE copy a shared variable");
        //                            } else {
        //                                LogError(op->getBeginLoc(), "Error copying shared variable due to lifetime extension");
        //                            }
        //                        }
        //                    }
        //                }
        //            }
        //            return true;
        //        }
        //        bool VisitReturnStmt(const ReturnStmt * ret) {
        //
        //            if (isEnabled() && ret) {
        //
        //                if (const ExprWithCleanups * inplace = dyn_cast_or_null<ExprWithCleanups>(ret->getRetValue())) {
        //                    LogOnly(inplace->getBeginLoc(), "Return inplace object");
        //                    return true;
        //                }
        //
        //                std::string retval = getArgName(ret->getRetValue());
        //                if (retval.empty()) {
        //                    LogOnly(ret->getBeginLoc(), "Return is not a variable");
        //                    return true;
        //                }
        //
        //                int retval_level;
        //                const char * retval_type = m_scopes.findVariable(retval, retval_level);
        //                if (retval_type == nullptr) {
        //                    LogError(ret->getBeginLoc(), "Return variable name not found!");
        //                    return true;
        //                }
        //
        //                if (std::string_view(AUTO_TYPE).compare(retval_type) == 0) {
        //                    if (m_scopes.testUnsafe().isValid()) {
        //                        LogWarning(ret->getBeginLoc(), "UNSAFE return auto variable");
        //                    } else {
        //                        LogOnly(ret->getBeginLoc(), "Return auto variable");
        //                    }
        //                } else if (std::string_view(SHARED_TYPE).compare(retval_type) == 0) {
        //                    if (m_scopes.testUnsafe().isValid()) {
        //                        LogWarning(ret->getBeginLoc(), "UNSAFE return shared variable");
        //                    } else {
        //                        LogError(ret->getBeginLoc(), "Return shared variable");
        //                    }
        //                }
        //            }
        //            return true;
        //        }
        //        bool VisitBinaryOperator(const BinaryOperator * op) {
        //
        //            if (isEnabled() && op->isAssignmentOp()) {
        //
        //                std::string lval;
        //                const char * lval_type;
        //                int lval_level;
        //                bool is_lval = checkArg(op->getLHS(), lval, lval_type, lval_level);
        //
        //
        //                std::string rval;
        //                const char * rval_type;
        //                int rval_level;
        //                bool is_rval = checkArg(op->getRHS(), rval, rval_type, rval_level);
        //
        //                if (is_lval && is_rval) {
        //                    if (std::string_view(SHARED_TYPE).compare(lval_type) == 0 && std::string_view(SHARED_TYPE).compare(rval_type) == 0) {
        //                        if (lval_level > rval_level) {
        //                            LogOnly(op->getBeginLoc(), "Copy of shared variable with shorter lifetime");
        //                        } else {
        //                            if (m_scopes.testUnsafe().isValid()) {
        //                                LogWarning(op->getBeginLoc(), "UNSAFE copy a shared variable");
        //                            } else {
        //                                LogError(op->getBeginLoc(), "Error copying shared variable due to lifetime extension");
        //                            }
        //                        }
        //                    }
        //                }
        //            }
        //            return true;
        //        }
        //  **************************************************************************************************
        //  **************************************************************************************************
        //  **************************************************************************************************

        bool VisitUnaryOperator(const UnaryOperator * unary) {

            if (isEnabled() && unary->getOpcode() == UnaryOperator::Opcode::UO_AddrOf) {

                //https://github.com/llvm/llvm-project/blob/main/clang/include/clang/AST/OperationKinds.def                
                //// [C++ 5.5] Pointer-to-member operators.
                //BINARY_OPERATION(PtrMemD, ".*")
                //BINARY_OPERATION(PtrMemI, "->*")
                //// C++20 [expr.spaceship] Three-way comparison operator.
                //BINARY_OPERATION(Cmp, "<=>")
                //
                //        // [C99 6.5.3.2] Address and indirection
                //UNARY_OPERATION(AddrOf, "&")
                //UNARY_OPERATION(Deref, "*")
                //// [C++ Coroutines] co_await operator
                //UNARY_OPERATION(Coawait, "co_await")

                // @todo Receiving a raw pointer is an error ?
                if (m_scopes.testInplaceCaller()) {
                    LogOnly(unary->getOperatorLoc(), "Inplace address arithmetic");
                } else {
                    LogError(unary->getOperatorLoc(), "Operator for address arithmetic");
                }
            }

            return true;
        }

        bool VisitFieldDecl(const FieldDecl * field) {

            if (isEnabled()) {

                // The type (class) of the variable that the plugin should analyze
                const CXXRecordDecl * class_decl = field->getType()->getAsCXXRecordDecl();
                const char * found_type = checkClassNameTracking(class_decl);

                bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(*field);

                if (class_decl) {
                    checkTypeName(field->getLocation(), class_decl->getQualifiedNameAsString(), is_unsafe);
                }
                // The name of the variable
                std::string var_name = field->getNameAsString();

                if (AUTO_TYPE == found_type) {

                    // @todo Is it possible to create temporary automatic variables as class fields whose instance lifetime can be any?

                    if (is_unsafe) {
                        LogWarning(field->getLocation(), std::format("UNSAFE create auto variabe as field {}:{}", var_name, found_type));
                    } else {
                        LogError(field->getLocation(), std::format("Create auto variabe as field {}:{}", var_name, found_type));
                    }


                } else if (field->getType()->isPointerType()) {

                    // any variant of a raw pointer or address is an error

                    if (is_unsafe) {
                        LogWarning(field->getLocation(), "UNSAFE field type raw pointer");
                    } else {
                        LogError(field->getLocation(), "Field type raw pointer");
                    }

                }
            }

            return true;
        }

        /*
         * 
         * RecursiveASTVisitor Traverse... template methods for the created plugin context
         * 
         * 
         * 
         * 
         * 
         */


#define TRAVERSE_CONTEXT( name ) \
        bool Traverse ## name (name * arg) { \
            if (isEnabledStatus()) { \
                m_scopes.PushScope(arg->getEndLoc(), arg); \
                RecursiveASTVisitor<MemSafePlugin>::Traverse ## name (arg); \
                m_scopes.PopScope(); \
            } \
            return true; \
        }


        TRAVERSE_CONTEXT(MemberExpr);

        TRAVERSE_CONTEXT(CallExpr);
        TRAVERSE_CONTEXT(CXXMemberCallExpr);
        TRAVERSE_CONTEXT(CXXOperatorCallExpr);
        TRAVERSE_CONTEXT(CXXTemporaryObjectExpr);

        /*
         * Creating a plugin context for classes
         */

        bool TraverseCXXRecordDecl(CXXRecordDecl * decl) {

            if (isEnabledStatus() && decl->hasDefinition()) {

                if (!scaner) {
                    m_scopes.PushScope(decl->getLocation(), decl, m_scopes.testUnsafe());
                }

                if (m_is_cyclic_analysis) {
                    try {

                        ClassListType used;
                        ClassListType fileds;

                        // Don't add the name of the class being checked the first time,
                        // so that circular dependencies can be detected
                        MakeUsedClasses(*decl, used, fileds);

                        if (!scaner) {
                            // If this is the first pass at creating a circular reference file, 
                            // it is not possible to analyze them now.
                            if (!testSharedType(*decl, used)) {
                                if (decl->hasDefinition()) {
                                    m_not_shared_class[decl->getQualifiedNameAsString()] = {decl, decl->getLocation()};
                                    LogOnly(decl->getLocation(), std::format("Class '{}' marked as not shared", decl->getQualifiedNameAsString()));
                                }
                            } else {
                                if (checkCycles(*decl, used, fileds)) {
                                    LogOnly(decl->getLocation(), std::format("Class '{}' checked for cyclic references", decl->getQualifiedNameAsString()));
                                }
                            }
                        }

                    } catch (...) {
                        return false;
                    }
                }

                RecursiveASTVisitor<MemSafePlugin>::TraverseCXXRecordDecl(decl);

                if (!scaner) {
                    m_scopes.PopScope();
                }

                return true;
            }

            return RecursiveASTVisitor<MemSafePlugin>::TraverseCXXRecordDecl(decl);
        }

        /*
         * 
         * Creating a plugin context based on a variable type
         *  
         */

        bool TraverseVarDecl(VarDecl * var) {

            if (isEnabled()) {

                bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(*var);
                const CXXRecordDecl *class_decl = var->getType()->getAsCXXRecordDecl();
                if (class_decl) {
                    checkTypeName(var->getLocation(), class_decl->getQualifiedNameAsString(), is_unsafe);
                }

                // The type (class) of a reference type variable depends on the data 
                // and may become invalid if the original data changes.

                const DeclRefExpr * dre = nullptr;

                if (Expr * init = var->getInit()) {
                    dre = dyn_cast<DeclRefExpr>(removeTempExpr(init));
                }

                if (!dre) {
                    dre = getInitializer(*var);
                }


                // The name of the variable
                std::string var_name = var->getNameAsString();

                // The type (class) of the variable that the plugin should analyze
                const char * found_type = checkClassNameTracking(class_decl);

                if (!found_type) {

                    // The type (class) of the variable does not require analysis
                    m_scopes.back().other.emplace(var_name);

                    if (var->getType()->isPointerType()) {
                        if (is_unsafe) {
                            LogWarning(var->getLocation(), "UNSAFE Raw address");
                        } else {
                            LogError(var->getLocation(), "Raw address");
                        }
                    }

                    if (var->getType()->isPointerType() || var->getType()-> isReferenceType()) {
                        if (dre) {
                            std::string depend_name = dre->getDecl()->getNameAsString();

                            m_scopes.back().dependent.emplace(var_name, depend_name);
                            m_scopes.back().blocker.emplace(depend_name, std::vector<SourceLocation>({dre->getLocation()}));

                            LogOnly(var->getLocation(), std::format("{}:raw-addr=>{}", var_name, depend_name));
                        }
                    }

                } else if (AUTO_TYPE == found_type) {


                    /*
                     * SD_FullExpression 	Full-expression storage duration (for temporaries).
                     * SD_Automatic 	Automatic storage duration (most local variables).
                     * 
                     * SD_Thread 	Thread storage duration.
                     * SD_Static 	Static storage duration.
                     * 
                     * SD_Dynamic 	Dynamic storage duration. 
                     */
                    if (var->getStorageDuration() == SD_Static) {
                        if (is_unsafe) {
                            LogWarning(var->getLocation(), std::format("UNSAFE create auto variabe as static {}:{}", var_name, found_type));
                        } else {
                            LogError(var->getLocation(), std::format("Create auto variabe as static {}:{}", var_name, found_type));
                        }
                    } else {
                        LogOnly(var->getLocation(), std::format("Var found {}:{}", var_name, found_type));
                    }

                    if (dre) {

                        std::string depend_name = dre->getDecl()->getNameAsString();

                        /* var_name is a reference type from the variable depend_name.
                         * 
                         * var_name can be used until the original variable depend_name is changed
                         * or it has a non-const method call that changes its internal state.
                         *
                         * The first use depend_name sets SourceLocation from the current position, 
                         * so that it is cleared in the first call to VisitDeclRefExpr.
                         * 
                         * After that, any attempt to access var_name will result in an error.
                         */

                        m_scopes.back().dependent.emplace(var_name, depend_name);
                        m_scopes.back().blocker.emplace(depend_name, std::vector<SourceLocation>({dre->getLocation()}));

                        LogOnly(var->getLocation(), std::format("{}:{}=>{}", var_name, found_type, depend_name));

                    } else {
                        LogError(var->getLocation(), std::format("Unknown depended type {}:{}", var_name, found_type));
                    }

                } else {

                    // The remaining types (classes) are used for safe memory management.
                    m_scopes.AddVarDecl(var, found_type);

                    LogOnly(var->getLocation(), std::format("Var found {}:{}", var_name, found_type));
                }
            }

            return RecursiveASTVisitor<MemSafePlugin>::TraverseVarDecl(var);
        }

        bool TraverseParmVarDecl(ParmVarDecl * param) {
            if (isEnabled()) {
                m_scopes.AddVarDecl(param, checkClassNameTracking(param->getType()->getAsCXXRecordDecl()));
            }
            return true;
        }

        /*
         * Traversing statements in AST
         */
        bool TraverseStmt(Stmt * stmt) {

            // Enabling and disabling planig is implemented via declarations, 
            // so statements are processed only after the plugin is activated.
            if (isEnabledStatus()) {

                // Check for dump AST tree
                if (const DeclStmt * decl = dyn_cast_or_null<DeclStmt>(stmt)) {
                    checkDumpFilter(decl->getSingleDecl());
                }
                printDumpIfEnabled(stmt);


                const AttributedStmt * attrStmt = dyn_cast_or_null<AttributedStmt>(stmt);
                const CompoundStmt * block = dyn_cast_or_null<CompoundStmt>(stmt);

                if (isEnabled() && (attrStmt || block)) {


                    m_scopes.PushScope(stmt->getEndLoc(), std::monostate(), checkUnsafeBlock(attrStmt));

                    RecursiveASTVisitor<MemSafePlugin>::TraverseStmt(stmt);

                    m_scopes.PopScope();

                    return true;

                }

                // Recursive traversal of statements only when the plugin is enabled
                RecursiveASTVisitor<MemSafePlugin>::TraverseStmt(stmt);
            }

            return true;
        }

        /*
         * All AST analysis starts with TraverseDecl
         * 
         */
        bool TraverseDecl(Decl * D) {

            checkDumpFilter(D);
            checkDeclAttributes(D);
            printDumpIfEnabled(D);

            if (const FunctionDecl * func = dyn_cast_or_null<FunctionDecl>(D)) {

                if (isEnabled() && func->isDefined()) {

                    m_scopes.PushScope(D->getLocation(), func, m_scopes.testUnsafe());

                    RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);

                    m_scopes.PopScope();
                    return true;
                }

            }

            // Enabling and disabling the plugin is implemented using declarations, 
            // so declarations are always processed.
            RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);

            return true;
        }

    };

    /*
     * 
     * 
     * 
     * 
     */

    class MemSafePluginASTConsumer : public ASTConsumer {
    public:

        void HandleTranslationUnit(ASTContext &context) override {

            context.getParentMapContext().setTraversalKind(clang::TraversalKind::TK_IgnoreUnlessSpelledInSource);
            plugin->TraverseDecl(context.getTranslationUnitDecl());

            if (scaner) {
                if (context.getDiagnostics().hasErrorOccurred()) {
                    fs::remove(scaner->m_file_name);
                    llvm::outs() << "The file '" << scaner->m_file_name << "' was not written due to errors in the source code!\n";
                } else {


                    /* 
                     * The circular reference analysis file provides information about all classes that are defined 
                     * in the current translation unit (this means that the same class can be found in several files 
                     * (i.e. translation units) at the same time).
                     * 
                     * For each class, all parent classes are saved and for fields, 
                     * only their types are saved if they are shared (or pointers/references)
                     */


                    MemSafeFile::ClassReadType classes;
                    MemSafePlugin::ClassListType parents;
                    MemSafePlugin::ClassListType fields;
                    MemSafeFile::ClassRead shared;

                    for (auto elem : plugin->m_shared_type) {
                        if (const CXXRecordDecl ** cls = std::get_if<const CXXRecordDecl *>(&elem.second)) {
                            // Keep only those classes that are defined in the current translation unit
                            if ((*cls)->hasDefinition()) {

                                parents.clear();
                                fields.clear();

                                plugin->MakeUsedClasses(**cls, parents, fields);

                                shared.parents.clear();
                                for (auto par_elem : parents) {
                                    shared.parents[par_elem.first] = MemSafeLogger::LocToStr(par_elem.second.second, context.getSourceManager());
                                }

                                plugin->reduceSharedList(fields, false);

                                shared.fields.clear();
                                for (auto fil_elem : fields) {
                                    shared.fields[fil_elem.first] = MemSafeLogger::LocToStr(fil_elem.second.second, context.getSourceManager());
                                }

                                shared.comment = MemSafeLogger::LocToStr((*cls)->getLocation(), context.getSourceManager());
                                classes[(*cls)->getQualifiedNameAsString()] = shared;

                            }
                        }
                    }

                    shared.parents.clear();
                    shared.fields.clear();
                    for (auto elem : plugin->m_not_shared_class) {
                        if (plugin->m_shared_type.find(elem.first) == plugin->m_shared_type.end()) {
                            shared.comment = "Not shared type at: ";
                            shared.comment += MemSafeLogger::LocToStr(elem.second.second, context.getSourceManager());
                            classes[elem.first] = shared;
                        }
                    }

                    scaner->WriteFile(classes);
                    llvm::outs() << "Write file '" << scaner->m_file_name << "' complete!\n";
                }
            }

            if (logger) {

                llvm::outs().flush();
                llvm::errs().flush();

                logger->Dump(llvm::outs());
                llvm::outs() << "\n";
                plugin->dump(llvm::outs());
            }
        }
    };

    /*
     * 
     * 
     * 
     * 
     */

    class MemSafePluginASTAction : public PluginASTAction {
    public:

        /* Three types of plugin execution:
         *
         * - In one pass for one translation unit (default).
         * When all type definitions are in one file or strong references are not used 
         * for forward declarations of types whose definitions are in other translation units.
         *
         * - The first pass when processing several translation units (when the circleref-write argument is specified).
         * Create a file of the list of shared data types without performing the actual AST analysis.
         *
         * - The second pass when processing several translation units (when the circleref-read argument is specified).
         * Perform direct analysis of the program source code analysis with control of recursive references,
         * the list of which must be in the file created during the first processing of all translation units.
         *
         */
        std::string m_shared_write;
        std::string m_shared_read;
        bool m_shared_disabled;

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {

            std::unique_ptr<MemSafePluginASTConsumer> obj = std::unique_ptr<MemSafePluginASTConsumer>(new MemSafePluginASTConsumer());

            if (m_shared_disabled) {

                plugin->setCyclicAnalysis(false);
                PrintColor(llvm::outs(), "Circular reference analysis disabled");

            } else if (!m_shared_write.empty()) {

                PrintColor(llvm::outs(), "Write the circular reference analysis data to file {}", m_shared_write);
                scaner = std::unique_ptr<MemSafeFile>(new MemSafeFile(m_shared_write, InFile.str()));

            } else if (!m_shared_read.empty()) {

                PrintColor(llvm::outs(), "Read the circular reference analysis data from {}", m_shared_read);
                MemSafeFile read(m_shared_read, InFile.str());

                // Read the list of classes present in other files (translation units).
                read.ReadFile(plugin->m_classes);
            }
            return obj;
        }

        template <class... Args>
        void PrintColor(raw_ostream & out, std::format_string<Args...> fmt, Args&&... args) {

            out << "\033[1;46;34m";
            out << std::format(fmt, args...);
            out << "\033[0m\n";
        }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {

            m_shared_disabled = false;
            plugin = std::unique_ptr<MemSafePlugin>(new MemSafePlugin(CI));

            llvm::outs().SetUnbuffered();
            llvm::errs().SetUnbuffered();

            std::string first;
            std::string second;
            size_t pos;
            for (auto &elem : args) {
                pos = elem.find("=");
                if (pos != std::string::npos) {
                    first = elem.substr(0, pos);
                    second = elem.substr(pos + 1);
                } else {
                    first = elem;
                    second = "";
                }

                std::string message = plugin->processArgs(first, second, SourceLocation());

                if (!message.empty()) {
                    if (first.compare("log") == 0) {

                        logger = std::unique_ptr<MemSafeLogger>(new MemSafeLogger(CI));
                        PrintColor(llvm::outs(), "Enable dump and process logger");

                    } else if (first.compare("circleref-disable") == 0) {

                        if (!m_shared_read.empty() || !m_shared_write.empty()) {
                            llvm::errs() << "Only one of the arguments 'circleref-read', 'circleref-write' or 'circleref-disable' is allowed!\n";
                            return false;
                        }
                        m_shared_disabled = true;

                    } else if (first.compare("circleref-write") == 0) {

                        if (!m_shared_read.empty() || m_shared_disabled) {
                            llvm::errs() << "Only one of the arguments 'circleref-read', 'circleref-write' or 'circleref-disable' is allowed!\n";
                            return false;
                        }

                        m_shared_write = second.empty() ? MemSafeFile::SHARED_SCAN_FILE_DEFAULT : second;

                    } else if (first.compare("circleref-read") == 0) {

                        if (!m_shared_write.empty() || m_shared_disabled) {
                            llvm::errs() << "Only one of the arguments 'circleref-read', 'circleref-write' or 'circleref-disable' is allowed!\n";
                            return false;
                        }

                        m_shared_read = second.empty() ? MemSafeFile::SHARED_SCAN_FILE_DEFAULT : second;

                    } else {
                        llvm::errs() << "Unknown plugin argument: '" << elem << "'!\n";
                        return false;
                    }
                } else {
                    if (first.compare("level") == 0) {
                        // ok
                    } else {
                        llvm::errs() << "The argument '" << elem << "' is not supported via command line!\n";
                        return false;
                    }
                }
            }

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A(TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE), "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<MemSafePluginASTAction> S(TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE), "Memory safety plugin");


