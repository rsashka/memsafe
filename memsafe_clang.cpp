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

#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Lex/PreprocessorOptions.h"

#pragma clang attribute push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-this-capture"

#include "clang/AST/ASTDumper.h"

#pragma clang attribute pop

using namespace clang;
using namespace clang::ast_matchers;


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
                //            } else if (found->second) {
                //                Log(loc, std::format("Error reprocessing attribute! {}", found->first.printToString(m_ci.getSourceManager())));
            } else {
                found->second = true;
            }
        }

        inline std::string LocToStr(const SourceLocation &loc) {
            std::string str = loc.printToString(m_ci.getSourceManager());
            size_t pos = str.find(' ');
            if (pos == std::string::npos) {
                return str;
            }
            return str.substr(0, pos);
        }

        void Dump(raw_ostream & out) {
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
     * https://www.goldsborough.me/c++/clang/llvm/tools/2017/02/24/00-00-06-emitting_diagnostics_and_fixithints_in_clang_tools/
     * 
     */

    class FixItRewriterOptions : public clang::FixItOptions {
    public:

        using super = clang::FixItOptions;

        // Constructor.
        //
        // The \p RewriteSuffix is the option from the command line.

        explicit FixItRewriterOptions(const std::string& RewriteSuffix = "")
        : RewriteSuffix(RewriteSuffix) {

            super::InPlace = false; //  True if files should be updated in place.
            super::FixWhatYouCan = false; // Whether to abort fixing a file when not all errors could be fixed.
            super::FixOnlyWarnings = false; // Whether to only fix warnings and not errors.
            super::Silent = true; // If true, only pass the diagnostic to the actual diagnostic consumer if it is an error or a fixit was applied as part of the diagnost
        }

        std::string RewriteFilename(const std::string& Filename, int& fd) override {
            fd = -1;

            return Filename + RewriteSuffix;
        }

    public:
        // The suffix appended to rewritten files.
        std::string RewriteSuffix;
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
        typedef std::variant<std::monostate, const FunctionDecl *, const CXXTemporaryObjectExpr *, const CallExpr *, const CXXMemberCallExpr *, const CXXOperatorCallExpr *, const MemberExpr *> ScopeType;
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

            static_assert(std::is_same_v < const CallExpr *, std::variant_alternative_t < 3, LifeTime::ScopeType>>);
            static_assert(std::is_same_v <const MemberExpr *, std::variant_alternative_t < 6, LifeTime::ScopeType>>);


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
                return call->getDirectCallee()-> getQualifiedNameAsString();
            }

            return "";
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

            std::string result("\n\n");
            if (loc.isValid()) {
                if (loc.isMacroID()) {
                    result += m_CI.getSourceManager().getExpansionLoc(loc).printToString(m_CI.getSourceManager());
                } else {
                    result += loc.printToString(m_CI.getSourceManager());
                }
                result += ": ";
            }
            result += "#print-dump ";
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

    class MemSafePlugin : public RecursiveASTVisitor<MemSafePlugin> {
    public:
        static inline const char * SHARED = MEMSAFE_KEYWORD_SHARED;
        static inline const char * AUTO = MEMSAFE_KEYWORD_AUTO;
        static inline const char * INVAL_TYPE = MEMSAFE_KEYWORD_INVALIDATE_TYPE;
        static inline const char * INVAL_FUNC = MEMSAFE_KEYWORD_INVALIDATE_FUNC;

        static inline const char * NONCONST_ARG = MEMSAFE_KEYWORD_NONCONST_ARG;
        static inline const char * NONCONST_METHOD = MEMSAFE_KEYWORD_NONCONST_METHOD;

        static inline const std::set<std::string> attArgs{SHARED, AUTO, INVAL_TYPE, INVAL_FUNC, NONCONST_ARG, NONCONST_METHOD};

        std::map<std::string, std::string> m_clsUse;


        // The first string arguments in the `memsafe` attribute for working and managing the plugin
        static inline const char * PROFILE = MEMSAFE_KEYWORD_PROFILE;
        static inline const char * STATUS = MEMSAFE_KEYWORD_STATUS;

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

        std::set<std::string> m_listFirstArg{PROFILE, UNSAFE, SHARED, AUTO, INVAL_TYPE, INVAL_FUNC, WARNING, ERROR, STATUS, BASELINE, NONCONST_ARG, NONCONST_METHOD, PRINT_AST, PRINT_DUMP};
        std::set<std::string> m_listStatus{STATUS_ENABLE, STATUS_DISABLE, STATUS_PUSH, STATUS_POP};

        /**
         * List of base classes that contain strong pointers to data 
         * (copying a variable does not copy the data,  only the reference 
         * and increments the ownership counter).
         */
        std::set<std::string> m_type_shared;
        /**
         * List of base classes that contain strong pointers to data 
         * that can only be created in temporary variables 
         * (which are automatically deallocated when they go out of scope).
         */
        std::set<std::string> m_type_auto;
        /**
         * List of base iterator types that must be tracked for strict control of dependent pointers.         
         */
        std::set<std::string> m_list_inval_type;
        /**
         * List of base iterator types that must be tracked for strict control of dependent pointers.         
         */
        std::set<std::string> m_list_inval_func;

        std::set<std::string> m_type_warning;
        std::set<std::string> m_type_error;

        std::vector<bool> m_status{false};


        clang::DiagnosticsEngine::Level m_level_non_const_arg;
        clang::DiagnosticsEngine::Level m_level_non_const_method;

        bool m_warning_only;
        std::string m_prefix;

        static const inline std::pair<std::string, std::string> pair_empty{std::make_pair<std::string, std::string>("", "")};


        bool is_fixit;
        int64_t line_base;
        int64_t line_number;

        LifeTimeScope m_scopes;

        memsafe::StringMatcher m_dump_matcher;
        SourceLocation m_dump_location;
        SourceLocation m_trace_location;

        const CompilerInstance &m_CI;

        MemSafePlugin(const CompilerInstance &instance) :
        m_CI(instance), m_scopes(instance), m_warning_only(false), m_prefix("##memsafe"), is_fixit(false), line_base(0), line_number(0) {
            // Zero level for static variables
            m_scopes.PushScope(SourceLocation(), std::monostate(), SourceLocation());

            m_warning_only = true;

        }

        inline clang::DiagnosticsEngine &getDiag() {
            return m_CI.getDiagnostics();
        }

        void clear() {
            //            MemSafePlugin empty(m_CI);
            //            std::swap(*this, empty);
        }

        void dump() {
            llvm::outs() << "#memsafe-dump\n";
            llvm::outs() << "#error: " << makeHelperString(m_type_error) << "\n";
            llvm::outs() << "#warning: " << makeHelperString(m_type_warning) << "\n";
            llvm::outs() << "#shared: " << makeHelperString(m_type_shared) << "\n";
            llvm::outs() << "#auto: " << makeHelperString(m_type_auto) << "\n";
            llvm::outs() << "#type: " << makeHelperString(m_list_inval_type) << "\n";
            llvm::outs() << "#func: " << makeHelperString(m_list_inval_func) << "\n";
            llvm::outs() << "\n";
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

        void LogViolation(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {


            if (logger) {
                logger->Log(loc, std::format("#vio #{} {}", hash.isValid() ? LogPos(hash) : LogPos(loc), str));
            }
        }

        void LogMessage(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
            if (logger) {
                logger->Log(loc, std::format("#msg #{} {}", hash.isValid() ? LogPos(hash) : LogPos(loc), str));
            }
        }

        void LogError(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
            if (logger) {
                logger->Log(loc, std::format("#err #{} {}", hash.isValid() ? LogPos(hash) : LogPos(loc), str));
            }
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
            } else if (str.compare(NOTE) == 0) {
                if (level) {
                    *level = clang::DiagnosticsEngine::Level::Note;
                }
                return true;
            } else if (str.compare(REMARK) == 0) {
                if (level) {
                    *level = clang::DiagnosticsEngine::Level::Remark;
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

        std::string processArgs(std::string_view first, std::string_view second) {

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

                static std::string first_list;
                if (first_list.empty()) {
                    first_list = makeHelperString(m_listFirstArg);
                }
                result = "Unknown argument '";
                result += first.begin();
                result += "'. Expected string argument from the following list: ";
                result += first_list;
            }

            static const char * LEVEL_ERROR_MESSAGE = "Required behavior not recognized! Allowed values: '"
                    MEMSAFE_KEYWORD_ERROR "', '" "', '" MEMSAFE_KEYWORD_WARNING "', '" MEMSAFE_KEYWORD_NOTE "', '"
                    MEMSAFE_KEYWORD_REMARK "' or '" MEMSAFE_KEYWORD_IGNORED "'.";

            if (result.empty()) {
                if (first.compare(STATUS) == 0) {
                    if (m_listStatus.find(second.begin()) == m_listStatus.end()) {
                        static std::string status_list;
                        if (status_list.empty()) {
                            status_list = makeHelperString(m_listStatus);
                        }
                        result = "Unknown second argument '";
                        result += first.begin();
                        result += "'. Expected string from the following list: ";
                        result += status_list;
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
                } else if (first.compare(PROFILE) == 0) {
                    if (!second.empty()) {
                        result = "Loading profile from file is not implemented!";
                    }
                    //                } else if (first.compare(UNSAFE) == 0) {
                    //                    m_listUnsafe.emplace(second.begin());
                } else if (first.compare(ERROR) == 0) {
                    m_type_error.emplace(second.begin());
                } else if (first.compare(WARNING) == 0) {
                    m_type_warning.emplace(second.begin());
                } else if (first.compare(SHARED) == 0) {
                    m_type_shared.emplace(second.begin());
                } else if (first.compare(AUTO) == 0) {
                    m_type_auto.emplace(second.begin());
                } else if (first.compare(INVAL_TYPE) == 0) {
                    m_list_inval_type.emplace(second.begin());
                } else if (first.compare(INVAL_FUNC) == 0) {
                    m_list_inval_func.emplace(second.begin());

                } else if (first.compare(NONCONST_ARG) == 0) {
                    if (!checkBehavior(second, &m_level_non_const_arg)) {
                        result = LEVEL_ERROR_MESSAGE;
                    }
                } else if (first.compare(NONCONST_METHOD) == 0) {
                    if (!checkBehavior(second, &m_level_non_const_method)) {
                        result = LEVEL_ERROR_MESSAGE;
                    }
                }
            }
            return result;
        }

        /**
         * Check if class name is in the list of monitored classes
         */

        const char * checkClassName(std::string_view type) {
            if (m_type_auto.find(type.begin()) != m_type_auto.end()) {
                return AUTO;
            } else if (m_type_shared.find(type.begin()) != m_type_shared.end()) {
                return SHARED;
            } else if (m_list_inval_type.find(type.begin()) != m_list_inval_type.end()) {
                return INVAL_TYPE;
            }
            return nullptr;
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
            return isEnabledStatus();
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

                            LogMessage(attrStmt->getBeginLoc(), "Unsafe statement", attrStmt->getBeginLoc());

                            return elem->getLocation();
                        }
                    }

                }
            }
            return SourceLocation();
        }

        clang::DiagnosticsEngine::Level getLevel(clang::DiagnosticsEngine::Level original) {
            SourceLocation loc = m_scopes.testUnsafe();
            if (loc.isValid()) {
                if (logger) {
                    logger->AttrComplete(loc);
                }
                LogMessage(loc, "Unsafe statement");
                return clang::DiagnosticsEngine::Level::Ignored;
            }
            if (!isEnabled()) {
                return clang::DiagnosticsEngine::Level::Ignored;
            } else if (original == clang::DiagnosticsEngine::Level::Error && m_warning_only) {
                return clang::DiagnosticsEngine::Level::Warning;
            }
            return original;
        }


#define FIXIT_DIAG(location, remark, type, ... ) \
            if(is_fixit){ m_CI.getDiagnostics().Report(location, m_CI.getDiagnostics().getCustomDiagID(getLevel(clang::DiagnosticsEngine::Remark), remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type, remark, m_CI.getSourceManager().getSpellingLineNumber(location) __VA_OPT__(,) __VA_ARGS__)));}

#define FIXIT_ERROR(location, remark, type, ... ) \
            m_CI.getDiagnostics().Report(location, m_CI.getDiagnostics().getCustomDiagID(getLevel(clang::DiagnosticsEngine::Error), remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type, remark, m_CI.getSourceManager().getSpellingLineNumber(location) __VA_OPT__(,) __VA_ARGS__)))

        std::string MakeFixitMessage(const std::string_view type, const std::string_view remark, int64_t num, const std::string_view msg = "") {
            std::string message("/* ");
            message += m_prefix;
            message += " #";
            message += std::to_string(num - line_base + line_number);
            //            message += " #";
            //            message += std::to_string(line_base);
            //            message += " #";
            //            message += std::to_string(line_number);
            message += " #";
            message += type.begin();
            message += " #";
            message += remark.begin();

            message += (msg.empty() ? "" : " ");
            message += msg.begin();

            message += " */ ";

            return message;
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


        void checkDeclAttributes(const Decl *decl) {
            // Check namespace annotation attribute
            // This check should be done first as it is used to enable and disable the plugin.

            if (!decl) {
                return;
            }

            AnnotateAttr * attr = decl->getAttr<AnnotateAttr>();
            auto attr_args = parseAttr(attr);

            if (attr_args != pair_empty) {

                std::string error_str = processArgs(attr_args.first, attr_args.second);

                if (!error_str.empty()) {

                    clang::DiagnosticBuilder DB = getDiag().Report(attr->getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Error, "Error detected: %0"));
                    DB.AddString(error_str);

                    LogViolation(attr->getLocation(), error_str);

                } else if (attr_args.first.compare(BASELINE) == 0) {
                    try {
                        SourceLocation loc = decl->getLocation();
                        if (loc.isMacroID()) {
                            loc = m_CI.getSourceManager().getExpansionLoc(loc);
                        }
                        line_base = getDiag().getSourceManager().getSpellingLineNumber(loc);
                        line_number = std::stoi(attr_args.second);

                        clang::DiagnosticBuilder DB = getDiag().Report(loc, getDiag().getCustomDiagID(
                                DiagnosticsEngine::Note, "Mark baseline %0"));
                        DB.AddString(std::to_string(line_number));

                    } catch (...) {
                        getDiag().Report(decl->getLocation(), getDiag().getCustomDiagID(
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

        void printDumpIfEnabled(const Stmt * stmt) {
            if (!stmt || !isEnabled() || m_dump_matcher.isEmpty() || skipLocation(m_dump_location, stmt->getBeginLoc())) {
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

        std::string getCXXTypeName(const Type * type_ptr) {
            std::string type_name;
            if (const CXXRecordDecl * decl = type_ptr ? type_ptr->getAsCXXRecordDecl() : nullptr) { // Only C++ classes are checked
                llvm::raw_string_ostream name(type_name);
                decl->printQualifiedName(name);
            }
            return type_name;
        }

        const char * checkClassNameTracking(const CXXRecordDecl * type) {
            const char * result = type ? checkClassName(type->getQualifiedNameAsString()) : nullptr;
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
                return dyn_cast<DeclRefExpr>(calle->getImplicitObjectArgument()->IgnoreUnlessSpelledInSource()->IgnoreImplicit());
            }
            return nullptr;
        }

        const Expr * removeTempExpr(const Expr * expr) {
            if (expr && (isa<ImplicitCastExpr>(expr) || isa<ExprWithCleanups>(expr) || isa<CXXBindTemporaryExpr>(expr))) {
                return expr->IgnoreUnlessSpelledInSource();
            }
            return expr;
        }

        const Expr * getExprInitializer(const VarDecl & var) {

            const Expr * result = removeTempExpr(var.getAnyInitializer());

            if (!result || isa<StringLiteral>(result) || isa<IntegerLiteral>(result)) {
                return nullptr;
            }

            if (isa<DeclRefExpr>(result) || isa<CXXMemberCallExpr>(result) || isa<CXXConstructExpr>(result) || isa<UnaryOperator>(result)) {
                return result;
            }

            FIXIT_DIAG(var.getLocation(), "Unknown VarDecl initializer", "unknown");
            var.getAnyInitializer()->dumpColor();

            return nullptr;

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

                //                llvm::outs() << "VisitDeclRefExpr: " << ref_name << "  " << caller << "\n";
                //                llvm::outs() << static_cast<int> (LifeTimeScope::testMode(m_scopes.back().scope)) << "\n\n\n\n";

                //                llvm::outs() << m_scopes.Dump(ref->getLocation(), "");

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

                            LogMessage(cur_location, std::format("Only constant method '{}' does not change data.", caller));
                            return true;

                        } else if (mode == LifeTimeScope::ModifyMode::EDIT_ONLY) {

                            LogMessage(cur_location, std::format("Only non constant method '{}' alway changed data.", caller));
                            found->second.push_back(cur_location);

                        } else if (mode == LifeTimeScope::ModifyMode::BOTH_MODE) {

                            if (m_level_non_const_method < clang::DiagnosticsEngine::Level::Warning) {
                                LogMessage(cur_location, std::format("Both methods '{}' for constant and non-constant objects tracking disabled!", caller));
                                return true;
                            } else if (m_level_non_const_method == clang::DiagnosticsEngine::Level::Warning) {
                                LogMessage(cur_location, std::format("Both methods '{}' for constant and non-constant objects warning only!", caller));
                                FIXIT_DIAG(cur_location, "Both methods '{}' for constant and non-constant objects warning only!", "warning");
                                return true;
                            } else if (m_level_non_const_method > clang::DiagnosticsEngine::Level::Warning) {
                                LogMessage(cur_location, std::format("Both methods '{}' for constant and non-constant objects tracking enabled!", caller));
                                // FIXIT_DIAG(ref->getLocation(), "Both methods '{}' for constant and non-constant objects tracking enabled!", "warning");
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

                    // llvm::outs() << "VisitDeclRefExpr: " << ref_name << "  " << caller << "\n";
                    // llvm::outs() << static_cast<int> (LifeTimeScope::testMode(m_scopes.back().scope)) << "\n\n";

                    auto block_found = m_scopes.findBlocker(depend_found->second);
                    if (block_found != LifeTime::BlockerEnd) {

                        //                        llvm::outs() << "check: " << m_scopes.getCalleeName() << "\n";
                        //                        llvm::outs() << m_scopes.Dump(ref->getLocation(), "");

                        if (!caller.empty()) {
                            if (m_list_inval_func.find(caller) != m_list_inval_func.end()) {
                                LogMessage(ref->getLocation(), std::format("Call {} '{}'", MEMSAFE_KEYWORD_INVALIDATE_FUNC, caller));
                            } else {
                                caller.clear();
                            }
                        }

                        if (block_found->second.empty()) {
                            LogMessage(ref->getLocation(), std::format("Depended {} corrected!", ref_name));
                        } else {

                            for (auto &elem : block_found->second) {
                                llvm::outs() << elem.printToString(m_CI.getSourceManager()) << ": using main variable '" << block_found->first << "'\n";
                            }

                            FIXIT_ERROR(ref->getLocation(), "Using the dependent variable after changing the main variable!", "error", block_found->second.back().printToString(m_CI.getSourceManager()));
                            LogError(ref->getLocation(), std::format("Using the dependent variable '{}' after changing the main variable '{}'!", block_found->first, ref_name));
                        }
                    }
                }

            }
            return true;
        }

        bool VisitReturnStmt(const ReturnStmt *ret) {

            if (isEnabled() && ret) {

                if (const ExprWithCleanups * inplace = dyn_cast_or_null<ExprWithCleanups>(ret->getRetValue())) {
                    FIXIT_DIAG(inplace->getBeginLoc(), "Return inplace", MEMSAFE_KEYWORD_APPROVED);
                    return true;
                }

                if (const CXXConstructExpr * expr = dyn_cast_or_null<CXXConstructExpr>(ret->getRetValue())) {

                    const char * found_type = checkClassName(expr->getType().getAsString());

                    if (found_type) {
                        if (std::string_view(AUTO).compare(found_type) == 0) {
                            FIXIT_ERROR(ret->getRetValue()->getExprLoc(), "Return auto type", "error");
                        } else if (std::string_view(SHARED).compare(found_type) == 0) {

                            FIXIT_ERROR(ret->getRetValue()->getExprLoc(), "Return share type", "error");
                        }
                    }
                }
            }
            return true;
        }

        bool VisitUnaryOperator(const UnaryOperator * unary) {

            if (unary->getOpcode() == UnaryOperator::Opcode::UO_AddrOf) {

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

                //                llvm::outs() << "m_scopes.testInplaceCaller(): " << m_scopes.testInplaceCaller() << "\n";

                if (m_scopes.testInplaceCaller()) {
                    LogMessage(unary->getOperatorLoc(), "Inplace address arithmetic");
                } else {

                    FIXIT_ERROR(unary->getOperatorLoc(), "Operator for address arithmetic", "error");
                    LogError(unary->getOperatorLoc(), "Operator for address arithmetic");
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
         * 
         * Creating a plugin context based on a variable type
         *  
         */

        bool TraverseVarDecl(VarDecl * var) {

            if (isEnabled()) {

                // The type (class) of the variable that the plugin should analyze
                const char * found_type = checkClassNameTracking(var->getType()->getAsCXXRecordDecl());

                // The name of the variable
                std::string var_name = var->getNameAsString();

                // llvm::outs() << "TraverseVarDecl: " << var_name << " -> " << found_type << "\n";
                // type_ptr->dump();

                if (!found_type) {

                    // The type (class) of the variable does not require analysis
                    m_scopes.back().other.emplace(var_name);

                } else if (INVAL_TYPE == found_type) {

                    // The type (class) of a reference type variable depends on the data 
                    // and may become invalid if the original data changes.


                    // FIXIT_DIAG(var->getLocation(), "Iterator found", MEMSAFE_KEYWORD_APPROVED);


                    const DeclRefExpr * dre = nullptr;

                    if (Expr * init = var->getInit()) {
                        dre = dyn_cast<DeclRefExpr>(init->IgnoreUnlessSpelledInSource()->IgnoreImplicit());
                    }

                    if (!dre) {
                        dre = getInitializer(*var);
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

                        // llvm::outs() << dre->getLocation().printToString(m_CI.getSourceManager()) << "\n";
                        // depend_name += "=>";
                        // depend_name += var_name;

                        LogMessage(var->getLocation(), std::format("{}:{}=>{}", var_name, found_type, depend_name));

                    } else {

                        FIXIT_ERROR(var->getLocation(), "Unknown depended type", "depend");
                        LogError(var->getLocation(), std::format("Unknown depended type {}:{}", var_name, found_type));
                    }

                } else {

                    // The remaining types (classes) are used for safe memory management.
                    m_scopes.AddVarDecl(var, found_type);

                    /*
                     * SD_FullExpression 	Full-expression storage duration (for temporaries).
                     * SD_Automatic 	Automatic storage duration (most local variables).
                     * 
                     * SD_Thread 	Thread storage duration.
                     * SD_Static 	Static storage duration.
                     * 
                     * SD_Dynamic 	Dynamic storage duration. 
                     */
                    if (AUTO == found_type && var->getStorageDuration() == SD_Static) {
                        FIXIT_ERROR(var->getLocation(), "Create auto variabe as static", "error");
                        LogError(var->getLocation(), std::format("Create auto variabe as static {}:{}", var_name, found_type));
                    } else {

                        LogMessage(var->getLocation(), std::format("Var found {}:{}", var_name, found_type));
                    }
                }
            }

            return RecursiveASTVisitor<MemSafePlugin>::TraverseVarDecl(var);
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

                if (attrStmt || block) {


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

        FixItRewriterOptions FixItOptions;

        /*
         * 
         * 
         * 
         * 
         */

        void HandleTranslationUnit(ASTContext &context) override {

            std::unique_ptr<clang::FixItRewriter> Rewriter;

            if (!FixItOptions.RewriteSuffix.empty()) {

                Rewriter = std::make_unique<clang::FixItRewriter>(context.getDiagnostics(),
                        context.getSourceManager(),
                        context.getLangOpts(),
                        &FixItOptions);

                assert(Rewriter);

                context.getDiagnostics().setClient(Rewriter.get(), /*ShouldOwnClient=*/false);

            }

            plugin->is_fixit = !!Rewriter;

            context.getParentMapContext().setTraversalKind(clang::TraversalKind::TK_IgnoreUnlessSpelledInSource);
            plugin->TraverseDecl(context.getTranslationUnitDecl());


            if (logger) {
                llvm::outs() << "\nLogger output:\n";
                logger->Dump(llvm::outs());

                llvm::outs() << "\n";
                plugin->dump();
            }

            if (Rewriter) {

                Rewriter->WriteFixedFiles();
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
        std::string fixit_file_ext;
        std::string m_marker;
    public:

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {

            std::unique_ptr<MemSafePluginASTConsumer> obj = std::unique_ptr<MemSafePluginASTConsumer>(new MemSafePluginASTConsumer());
            if (!m_marker.empty()) {
                plugin->m_prefix = m_marker;
            }
            obj->FixItOptions.RewriteSuffix = fixit_file_ext;

            return obj;
        }

        template <class... Args>
        void PrintColor(raw_ostream & out, std::format_string<Args...> fmt, Args&&... args) {

            out << "\033[1;46;34m";
            out << std::format(fmt, args...);
            out << "\033[0m\n";
        }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {

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

                std::string message = plugin->processArgs(first, second);

                if (!message.empty()) {
                    if (first.compare("fixit") == 0) {

                        fixit_file_ext = second;
                        if (fixit_file_ext.empty()) {
                            llvm::errs() << "To enable FixIt output to a file, you must specify the extension of the file being created!\n";
                            return false;
                        }
                        if (fixit_file_ext[0] != '.') {
                            fixit_file_ext.insert(0, ".");
                        }
                        PrintColor(llvm::outs(), "Enabled FixIt output to a file with the extension: '{}'", fixit_file_ext);

                    } else if (first.compare("marker") == 0) {

                        if (second.empty()) {
                            llvm::errs() << "Debug trace marker cannot be empty!\n";
                            return false;
                        }
                        m_marker = second;
                        PrintColor(llvm::outs(), "Using debug trace marker: '{}'", m_marker);

                    } else if (first.compare("log") == 0) {

                        logger = std::unique_ptr<MemSafeLogger>(new MemSafeLogger(CI));
                        PrintColor(llvm::outs(), "Enable dump and process logger");

                    } else {
                        llvm::errs() << "Unknown plugin argument: '" << elem << "'!\n";
                        return false;
                    }
                } else {
                    llvm::errs() << "The argument '" << elem << "' is not supported via command line!\n";
                    return false;
                }
            }

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A(TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE), "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<MemSafePluginASTAction> S(TO_STR(MEMSAFE_KEYWORD_ATTRIBUTE), "Memory safety plugin");


