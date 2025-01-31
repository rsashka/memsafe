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

using namespace clang;
using namespace clang::ast_matchers;


namespace {


    /*
     * Класс MemSafePlugin сделан как шаблон RecursiveASTVisitor по следующим причинам:
     * - Обход AST происходит сверху вниз через все уровни вложености, что позволяет 
     * динамически создать и очищать уже не нужную контекстную информацию для функций и классов.
     * При поиске соотвестсвтий с помощью AST Matcher MatchCallback вызываются для найденнх узлов,
     * но создавать и очищать нужную контекстную информацию при каждом вызова очень накладно.
     * - RecursiveASTVisitor позволяет прерывать (или повторять) обход отдельных ветвей AST 
     * в зависимоости от динамической контекстной информации.
     * - Matcher обработывает AST только для конкретных указанных сопоставителей,
     * и в случае появления нетривиальных сценариев, некоторые из них могут быть пропущены,
     * тогда как RecursiveASTVisitor последовательно обходит все узлы AST.
     * 
     * Плагин используется как статический сигнлтон для хранения контекст и упрощения доступа из любых классов,
     * но создается динамически, чтобы использовать контктеный CompilerInstance
     */

    class MemSafePlugin;
    static std::unique_ptr<MemSafePlugin> plugin;

    /*
     * 
     * 
     * 
     * 
     */

    struct MemSafeAttrInfo : public ParsedAttrInfo {

        MemSafeAttrInfo() {
            // Can take up to 15 optional arguments, to emulate accepting a variadic
            // number of arguments. This just illustrates how many arguments a
            // `ParsedAttrInfo` can hold, we will not use that much in this example.

            OptArgs = 8;
            //            IsType = true;
            // GNU-style __attribute__(("memsafe")) and C++/C23-style [[memsafe]] and
            // [[::memsafe]] supported.
            static constexpr Spelling S[] = {
                {ParsedAttr::AS_GNU, "memsafe"},
                {ParsedAttr::AS_C23, "memsafe"},
                {ParsedAttr::AS_CXX11, "memsafe"},
                {ParsedAttr::AS_CXX11, "::memsafe"},
            };
            Spellings = S;
        }

        bool diagAppertainsToDecl(Sema &S, const ParsedAttr &Attr, const Decl *D) const override {
            return isa<EmptyDecl>(D) || isa<NamespaceDecl>(D) || isa<CXXRecordDecl>(D);
        }

        AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &Attr) const override {

            if (Attr.getNumArgs() > 2) {

                unsigned ID = S.getDiagnostics().getCustomDiagID(
                        DiagnosticsEngine::Error,
                        "'memsafe' attribute accept only one argument");
                S.Diag(Attr.getLoc(), ID);
                return AttributeNotApplied;
            }

            //            llvm::outs() << "Attr.getNumArgs() " << Attr.getNumArgs() << "\n";

            // If there are arguments, the first argument should be a string literal.
            if (Attr.getNumArgs() > 0) {
                auto *Arg0 = Attr.getArgAsExpr(0);
                StringLiteral *Literal = dyn_cast<StringLiteral>(Arg0->IgnoreParenCasts());
                if (!Literal) {
                    unsigned ID = S.getDiagnostics().getCustomDiagID(
                            DiagnosticsEngine::Error, "argument to the 'memsafe' "
                            "attribute must be a string literal");
                    S.Diag(Attr.getLoc(), ID);
                    return AttributeNotApplied;
                }

                SmallVector<Expr *, 16> ArgsBuf;
                for (unsigned i = 0; i < Attr.getNumArgs(); i++) {
                    ArgsBuf.push_back(Attr.getArgAsExpr(i));
                }

                D->addAttr(AnnotateAttr::Create(S.Context, "memsafe", ArgsBuf.data(),
                        ArgsBuf.size(), Attr.getRange()));
            } else {

                // Attach an annotate attribute to the Decl.

                D->addAttr(AnnotateAttr::Create(S.Context, "memsafe", nullptr, 0,
                        Attr.getRange()));
            }

            //            llvm::outs() << "AttributeApplied !!!!\n";
            return AttributeApplied;
        }

        /*
         * @todo clang-20
         * To apply custom attributes to expressions, you need to update clang, 
         * as the current release version (19) 
         * does not have this functionality (handleStmtAttribute)
         * 
         */

        bool diagAppertainsToStmt(Sema &S, const ParsedAttr &Attr, const Stmt *St) const override {
            //            St->dumpColor();
            //            llvm::outs() << "diagAppertainsToStmt " << isa<ReturnStmt>(St) << "\n";
            //            llvm::outs().flush();
            return true; //isa<ReturnStmt>(St);
        }
        //
        //        AttrHandling handleStmtAttribute(Sema &S, Stmt *St, const ParsedAttr &Attr) const override {
        //
        //            llvm::outs() << "handleStmtAttribute\n";
        //            llvm::outs().flush();
        //            St->dumpColor();
        //
        //            return AttributeApplied;
        //            if (const ReturnStmt * ret = dyn_cast_or_null<ReturnStmt>(St)) {
        //
        //                if (Attr.getNumArgs() != 1) {
        //
        //                    unsigned ID = S.getDiagnostics().getCustomDiagID(
        //                            DiagnosticsEngine::Error,
        //                            "'memsafe' attribute accept only one argument");
        //                    S.Diag(Attr.getLoc(), ID);
        //                    return AttributeNotApplied;
        //                }
        //
        //                auto *Arg0 = Attr.getArgAsExpr(0);
        //                StringLiteral *Literal = dyn_cast<StringLiteral>(Arg0->IgnoreParenCasts());
        //                if (!Literal) {
        //                    unsigned ID = S.getDiagnostics().getCustomDiagID(
        //                            DiagnosticsEngine::Error, "argument to the 'memsafe' "
        //                            "attribute must be a string literal");
        //                    S.Diag(Attr.getLoc(), ID);
        //                    return AttributeNotApplied;
        //                }
        //
        //                Expr * expr = Attr.getArgAsExpr(0);
        //
        //                expr->dumpColor();
        //
        //                SmallVector<Expr *, 16> ArgsBuf;
        //                for (unsigned i = 0; i < Attr.getNumArgs(); i++) {
        //                    ArgsBuf.push_back(Attr.getArgAsExpr(i));
        //                }
        //
        //                //                St->addAttr(AnnotateAttr::Create(S.Context, "memsafe", ArgsBuf.data(), ArgsBuf.size(), Attr.getRange()));
        //                return AttributeApplied;
        //            }
        //
        //            return AttributeNotApplied;
        //        }

    };


    /*
     * 
     * 
     * 
     * 
     * 
     */

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

    struct VarInfo {
        static const int NOLEVEL = std::numeric_limits<int>::lowest();
        typedef std::variant<std::monostate, const FunctionDecl *, const FieldDecl *> DeclType;

        const VarDecl &var;
        const char * const &type;
        /**
         * Lexical level variable:
         * 0 - static variables
         * positive numbers - nesting level of the scope
         * negative numbers - the level of data nesting (structures and classes taking into account all parents)
         */
        const int level;
        /**
         * Name from AST with function name (for automatic variables)
         * or class/structure name for field names
         * Null pointer for static variables
         */
        DeclType parent;
    };

    struct DeclInfo {
        const ValueDecl * decl;
        const char * type;
    };

    struct ScopeLevel {
        /** 
         * Map of lvalues for context (variable in namespace, function args or fields in class)
         */
        std::map<std::string, DeclInfo> lval;
        /** 
         * Map of rvalues using for context (temp values in expresion and args for function calls)
         */
        //        std::map<std::string, const DeclRefExpr *> rval;

        /** 
         * Map of iterators and dependent variables (iter => value)
         */
        typedef std::map<std::string, std::string> DependentType;
        DependentType dependent;
        static inline const DependentType::iterator DependentEnd = static_cast<DependentType::iterator> (nullptr);

        /** 
         * Map of dependent variable usage and their possible changes 
         * (data modifications that can invalidate the iterator)
         * (value => Location)
         */
        typedef std::map<std::string, SourceLocation> BlockerType;
        BlockerType blocker;
        static inline const BlockerType::iterator BlockerEnd = static_cast<BlockerType::iterator> (nullptr);

        const FunctionDecl *func; // current function or null

        ScopeLevel(const FunctionDecl *f = nullptr) : func(f) {

        }
    };

    class MemSafePlugin : public RecursiveASTVisitor<MemSafePlugin> {
    public:
        // A list of attribute parameters that the memory safety model will work with, 
        // as well as the names of the class templates that are used to implement each data type.
        //     
        // Template class names are missing until they are defined during plugin initialization 
        // in a namespace with the memsafe attribute and the 'define' argument 
        // (i.e. namespace [[memsafa("define")]] ... )

        //        static inline const char * VALUE = "value";
        static inline const char * SHARED = "shared";
        static inline const char * AUTO = "auto";
        static inline const char * INVAL = "invalidate";


        static inline const std::set<std::string> attArgs{SHARED, AUTO, INVAL};

        //        static inline const std::set<std::string> attrDef{defError.data(), defWarning.data(), defSafe.data(), defUnsafe.data()};

        std::map<std::string, std::string> m_clsUse;


        // The first string arguments in the `memsafe` attribute for working and managing the plugin
        static inline const char * PROFILE = "profile";
        static inline const char * STATUS = "status";
        static inline const char * ERROR = "error";
        static inline const char * WARNING = "warning";
        static inline const char * LINE = "line";
        static inline const char * UNSAFE = "unsafe";

        static inline const char * STATUS_ENABLE = "enable";
        static inline const char * STATUS_DISABLE = "disable";
        static inline const char * STATUS_PUSH = "push";
        static inline const char * STATUS_POP = "pop";

        //        static inline const char * MODE_ERROR = "error"; ///< Полная диагностика кода плагином с выдачей предупреждеенйи и ошибок
        //        static inline const char * MODE_WARNING = "warning"; ///< Полная диагностика кода плагином, но в место ошибок выводятся предупреждения
        //        static inline const char * MODE_DEBUG = "debug";
        //        static inline const char * MODE_DUMP = "dump";


        std::set<std::string> m_listFirstArg{PROFILE, UNSAFE, SHARED, AUTO, INVAL, WARNING, ERROR, STATUS, LINE};
        std::set<std::string> m_listStatus{STATUS_ENABLE, STATUS_DISABLE, STATUS_PUSH, STATUS_POP};

        std::set<std::string> m_listUnsafe;
        std::set<const NamespaceDecl *> m_safeSet;
        std::set<const NamespaceDecl *> m_unsafeSet;

        /**
         * List of base classes that contain strong pointers to data 
         * (copying a variable does not copy the data,  only the reference 
         * and increments the ownership counter).
         */
        std::set<std::string> m_listShared;
        /**
         * List of base classes that contain strong pointers to data 
         * that can only be created in temporary variables 
         * (which are automatically deallocated when they go out of scope).
         */
        std::set<std::string> m_listAuto;
        /**
         * List of base iterator types that must be tracked for strict control of dependent pointers.         
         */
        std::set<std::string> m_listIter;

        std::set<std::string> m_listWarning;
        std::set<std::string> m_listError;
        std::vector<bool> m_status{false};

        std::set<std::string> m_is_template;
        std::map<std::string, const CXXRecordDecl *> m_class_decl;

        const CompilerInstance &m_CI;

        MemSafePlugin(const CompilerInstance &instance) :
        m_CI(instance), m_is_unsafe(false), m_warning_only(false), m_prefix("##memsafe"), is_fixit(false), line_base(0), line_number(0) {
            // Zero level for static variables
            m_scopes.push_back(ScopeLevel());

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
            llvm::outs() << "m_listError: " << makeHelperString(m_listError) << "\n";
            llvm::outs() << "m_listWarning: " << makeHelperString(m_listWarning) << "\n";
            llvm::outs() << "m_listShared: " << makeHelperString(m_listShared) << "\n";
            llvm::outs() << "m_listAuto: " << makeHelperString(m_listAuto) << "\n";
            llvm::outs() << "m_listIter: " << makeHelperString(m_listIter) << "\n";
            llvm::outs() << "m_varOther: " << makeHelperString(m_varOther) << "\n";

            //            llvm::outs() << "m_iter_depend: ";
            //            for (auto &elem : m_iter_depend) {
            //                llvm::outs() << elem.first << ">>" << elem.second << " ";
            //            }
            //            llvm::outs() << "\n";
            //
            //            llvm::outs() << "m_iter_stoper: ";
            //            for (auto &elem : m_iter_stoper) {
            //                if (elem.second.isValid()) {
            //                    llvm::outs() << elem.first << " ";
            //                }
            //            }
            llvm::outs() << "\n";

            // llvm::outs() << "m_listUnsafe: " << makeHelperString(m_listUnsafe) << "\n";
        }

        /**
         * Add pointer to unsafe namespace
         */
        void AddUnsafe(const NamespaceDecl *ns) {
            assert(ns);
            m_unsafeSet.insert(ns);
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

        std::string processArgs(std::string_view first, std::string_view second) {

            std::string result;
            if (first.empty() || second.empty()) {
                if (first.compare(PROFILE) == 0) {
                    clear();
                } else if (first.compare(UNSAFE) == 0) {
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
                } else if (first.compare(UNSAFE) == 0) {
                    m_listUnsafe.emplace(second.begin());
                } else if (first.compare(ERROR) == 0) {
                    m_listError.emplace(second.begin());
                } else if (first.compare(WARNING) == 0) {
                    m_listWarning.emplace(second.begin());
                } else if (first.compare(SHARED) == 0) {
                    m_listShared.emplace(second.begin());
                } else if (first.compare(AUTO) == 0) {
                    m_listAuto.emplace(second.begin());
                } else if (first.compare(INVAL) == 0) {
                    m_listIter.emplace(second.begin());
                }
            }
            return result;
        }

        /**
         * Check if class name is in the list of monitored classes
         */

        inline const char * checkClassName(std::string_view type) {
            if (m_listAuto.find(type.begin()) != m_listAuto.end()) {
                return AUTO;
            } else if (m_listShared.find(type.begin()) != m_listShared.end()) {
                return SHARED;
            } else if (m_listIter.find(type.begin()) != m_listIter.end()) {
                return INVAL;
            }
            return nullptr;
        }

        //        inline const char * checkClassNameIter(std::string_view type) {
        //            if (m_listIter.find(type.begin()) != m_listIter.end()) {
        //                return INVAL;
        //            }
        //            return nullptr;
        //        }

        inline bool isEnabledStatus() {
            assert(!m_status.empty());
            return *m_status.rbegin();
        }


        //        CompilerInstance &Instance;
        bool m_is_unsafe;
        bool m_warning_only;
        std::string m_prefix;

        static const inline std::pair<std::string, std::string> pair_empty{std::make_pair<std::string, std::string>("", "")};

        typedef std::multimap<std::string, VarInfo> VAR_LIST_TYPE;
        typedef std::multimap<std::string, VarInfo>::iterator VAR_LIST_INVAL;
        VAR_LIST_TYPE m_varList;
        std::set<std::string> m_varOther;

        /**
         * Add variable name to watchlist
         */
        void AddVariable(const VarDecl &var, const char * const &type, const int level, VarInfo::DeclType parent = std::monostate()) {

            llvm::outs() << "AddVariable: " << var.getNameAsString() << " !!!\n";

            //            if (m_varList.find(var.getQualifiedNameAsString()) != m_varList.end()) {
            //                //  llvm::outs() << "AddVariable dublicate: " << name << "  " << type << " !!!\n";
            //            }
            m_varList.emplace(var.getNameAsString(), VarInfo{var, type, level, parent});
            //            m_varList.emplace(var.getQualifiedNameAsString(), VarInfo{var, type, level, parent});
        }


        bool is_fixit;
        int64_t line_base;
        int64_t line_number;

        std::vector<ScopeLevel> m_scopes;

        std::vector<const VarDecl *> m_decl;
        std::vector<const CompoundStmt *> m_level;

        inline bool isEnabled() {
            return isEnabledStatus() && !m_is_unsafe;
        }

        clang::DiagnosticsEngine::Level getLevel(clang::DiagnosticsEngine::Level original) {
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

            //            if (level != VarInfo::NOLEVEL) {
            //                message += " #";
            //                message += std::to_string(level);
            //            }
            //
            //            if (const FunctionDecl * const * func = std::get_if<const FunctionDecl*> (&parent)) {
            //
            //                message += " #\"";
            //                message += (*func)->getReturnType().getAsString();
            //                message += " ";
            //                message += (*func)->getQualifiedNameAsString();
            //                message += "(";
            //                for (int i = 0; i < (*func)->getNumParams(); i++) {
            //                    if ((*func)->getNumParams() > 1) {
            //                        message += ", ";
            //                    }
            //                    message += (*func)->parameters()[i]->getOriginalType().getAsString();
            //                    message += " ";
            //                    message += (*func)->parameters()[i]->getQualifiedNameAsString();
            //                }
            //                message += ")\"";
            //
            //            } else if (const FieldDecl * const * field = std::get_if<const FieldDecl *> (&parent)) {
            //                message += ", \"class ";
            //                message += (*field)->getNameAsString();
            //                message += "\"";
            //            }

            message += " */ ";

            return message;
        }

        std::pair<std::string, std::string> parseAttr(AnnotateAttr *attr) {
            if (!attr || attr->getAnnotation() != "memsafe" || attr->args_size() != 2) {
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

            std::string error_str = processArgs(first->getString().str(), second->getString().str());
            if (!error_str.empty()) {
                clang::DiagnosticBuilder DB = getDiag().Report(attr->getLocation(), getDiag().getCustomDiagID(
                        DiagnosticsEngine::Error, "Error detected: %0"));
                DB.AddString(error_str);
                return pair_empty;
            }
            return std::make_pair < std::string, std::string>(first->getString().str(), second->getString().str());
        }

        /*
         * 
         * 
         * 
         */


        bool checkDeclAttributes(const Decl *ns) {
            // Check namespace annotation attribute
            // This check should be done first as it is used to enable and disable the plugin.

            if (!ns) {
                return false;
            }

            AnnotateAttr * attr = ns->getAttr<AnnotateAttr>();
            auto attr_args = parseAttr(attr);

            if (attr_args != pair_empty) {

                if (attr_args.first.compare(LINE) == 0) {
                    try {
                        line_number = std::stoi(attr_args.second);
                        if (ns->getLocation().isMacroID()) {
                            line_base = getDiag().getSourceManager().getSpellingLineNumber(m_CI.getSourceManager().getExpansionLoc(ns->getLocation()));
                        } else {
                            line_base = m_CI.getSourceManager().getSpellingLineNumber(attr->getLocation());
                        }
                    } catch (...) {
                        getDiag().Report(ns->getLocation(), getDiag().getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "The second argument is expected to be a line number as a literal string!"));
                    }
                } else if (attr_args.first.compare(STATUS) == 0) {

                    clang::DiagnosticBuilder DB = getDiag().Report(ns->getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Remark,
                            "Status memory safety plugin is %0!"));
                    DB.AddString(attr_args.second);

                }
            }
            return true;
        }

        /*
         * 
         * 
         * 
         * 
         */
        bool checkUnsafe(const Decl & decl) {

            clang::DynTypedNodeList parents = m_CI.getASTContext().getParents(decl);

            while (!parents.empty()) {
                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                }

                if (const NamespaceDecl * ns = parents[0].get<NamespaceDecl>()) {
                    //@todo Rewrite to check attribute
                    if (m_unsafeSet.find(ns) != m_unsafeSet.end()) {
                        FIXIT_DIAG(decl.getBeginLoc(), "Add mark \"unsafe\" namesapace", "unsafe");
                        return true;
                    }
                }

                parents = m_CI.getASTContext().getParents(parents[0]);
            }
            return false;
        }

        VarInfo::DeclType findParenName(clang::DynTypedNodeList parents, int & level) {

            level++;

            while (!parents.empty()) {

                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                }

                if (const FunctionDecl * func = parents[0].get<FunctionDecl>()) {
                    return func;
                }

                if (parents[0].get<CompoundStmt>()) {
                    // CompoundStmt - определение внутри блока кода

                    level++;
                    goto next_parent;
                }

next_parent:
                ;

                parents = m_CI.getASTContext().getParents(parents[0]);
            }
            return std::monostate();
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

        const char * expandAssignArgs(const Expr * expr, const DeclRefExpr * &decl, VAR_LIST_INVAL & iter) {
            decl = nullptr;
            iter = m_varList.end();

            if (!expr) {
                return nullptr;
            }

            decl = dyn_cast_or_null<DeclRefExpr>(expr->IgnoreUnlessSpelledInSource());
            if (!decl) {
                return nullptr;
            }

            std::string type_name;
            const ValueDecl *value = decl->getDecl();
            if (value) {
                type_name = getCXXTypeName(value->getType().getTypePtrOrNull());
            } else {
                type_name = getCXXTypeName(expr->getType().getTypePtrOrNull());
            }

            const char * name = checkClassName(type_name);

            llvm::errs() << name << "  " << type_name << " !!!!!!!!!!!!!!\n\n";

            iter = m_varList.find(decl->getNameInfo().getAsString());

            if (name) {
                if (iter == m_varList.end()) {
                    decl->dumpColor();
                    for (auto &elem : m_varList) {
                        llvm::errs() << elem.first;
                        llvm::errs() << " ";
                    }
                    llvm::errs() << "\n";

                    FIXIT_ERROR(decl->getLocation(), "Variable detected ERROR", "error", decl->getNameInfo().getAsString());
                } else {

                    FIXIT_DIAG(decl->getLocation(), "Variable detected", name); //, iter->second.level);
                }

            } else {
                if (m_varOther.find(decl->getNameInfo().getAsString()) == m_varOther.end()) {
                    llvm::errs() << " !!!!!!!!!!!!!!!!!!!!!!!!! " << name << "  " << type_name << " !!!!!!!!!!!!!!\n\n";
                    decl->dumpColor();
                    FIXIT_ERROR(decl->getLocation(), "Other variable detected ERROR", "error", decl->getNameInfo().getAsString());
                }
            }
            return name;
        }

        /*
         * 
         * 
         */

        bool VisitCXXRecordDecl(const CXXRecordDecl *decl) {
            if (m_is_template.find(decl->getQualifiedNameAsString()) == m_is_template.end() && decl->hasDefinition()) {
                /* 
                 * if there are multiple implementations of a class with the same name 
                 * (when specializing on the same template or when forward-declaring), 
                 * only the latest version is stored, 
                 * which is sufficient for analyzing the class interface                 
                 */
                m_class_decl.emplace(decl->getQualifiedNameAsString(), decl);
            }
            return true;
        }

        bool VisitClassTemplateDecl(const ClassTemplateDecl *decl) {
            if (decl->isThisDeclarationADefinition()) {
                /*
                 * When we pre-declare a class specialization from a template, 
                 * the class declaration already exists, but we only need the latest version 
                 * of the template itself to parse its interface methods.
                 */
                m_is_template.emplace(decl->getQualifiedNameAsString());
                m_class_decl.emplace(decl->getQualifiedNameAsString(), decl->getTemplatedDecl());
            }
            return true;
        }

        const DeclRefExpr * getInitializer(const VarDecl &decl) {
            if (const CXXMemberCallExpr * calle = dyn_cast_or_null<CXXMemberCallExpr>(getExprInitializer(decl))) {
                return dyn_cast<DeclRefExpr>(calle->getImplicitObjectArgument()->IgnoreUnlessSpelledInSource());
            }
            return nullptr;
        }

        ScopeLevel::DependentType::iterator findDependent(const std::string_view name) {
            for (auto level = m_scopes.rbegin(); level != m_scopes.rend(); level++) {
                auto found = level->dependent.find(name.begin());
                if (found != level->dependent.end()) {
                    return found;
                }
            }
            return ScopeLevel::DependentEnd;
        }

        ScopeLevel::BlockerType::iterator findBlocker(const std::string_view name) {
            for (auto level = m_scopes.rbegin(); level != m_scopes.rend(); level++) {
                auto found = level->blocker.find(name.begin());
                if (found != level->blocker.end()) {
                    return found;
                }
            }
            return ScopeLevel::BlockerEnd;
        }

        /** 
         * Map of dependent variable usage and their possible changes 
         * (data modifications that can invalidate the iterator)
         * (value => Location)
         */

        bool VisitVarDecl(const VarDecl *var) {
            // VarDecl - Represents a variable declaration or definition
            if (isEnabled()) {
                std::string var_name = var->getNameAsString();
                const char * found_type = checkClassName(getCXXTypeName(var->getType().getTypePtrOrNull()));

                llvm::outs() << "VarDecl: " << var_name << " " << found_type << "\n";

                auto data = m_scopes.back().lval.emplace(var_name, DeclInfo({var, found_type}));

                if (INVAL == found_type) {

                    FIXIT_DIAG(var->getLocation(), "Iterator found", "approved");

                    if (const DeclRefExpr * dre = getInitializer(*var)) { //dyn_cast<DeclRefExpr>(calle->getImplicitObjectArgument()->IgnoreUnlessSpelledInSource())

                        std::string depend_name = dre->getDecl()->getNameAsString();

                        //                        llvm::outs() << "Depended exists: " << depend_name << " !\n";
                        //
                        //                        for (auto level = m_scopes.rbegin(); level != m_scopes.rend(); level++) {
                        //                            auto found = level->depend.find(depend_name);
                        //                            if (found != level->depend.end()) {
                        //
                        //                                llvm::outs() << "Depended exists: " << depend_name << " !\n";
                        //
                        //                                for (auto stop_level = m_scopes.rbegin(); stop_level != m_scopes.rend(); stop_level++) {
                        //                                    auto stop_found = stop_level->stoper.find(found->second);
                        //                                    if (stop_found == stop_level->stoper.end()) {
                        //                                        FIXIT_ERROR(var->getLocation(), "Logic error!", "error", found->second);
                        //                                    } else if (stop_found->second.isValid()) {
                        //                                        FIXIT_ERROR(stop_found->second, "Using the dependent variable after changing the main variable!", "error", found->second);
                        //                                    } else {
                        //                                        llvm::outs() << "Depended corrected!\n";
                        //                                    }
                        //                                }
                        //                                return true;
                        //                            }
                        //                        }


                        /* var_name - итератор объекта из переменной depend_name.
                         * Итератор можно использовать пока не будет изменена исходная переменная
                         * или вызов у нее не константного метода.
                         * 
                         * Первое изменение к decl_name устанавливает SourceLocation
                         * из текщуей позиции, чтобы её очисить в вызове VisitDeclRefExpr.
                         * После этого любая попытка обратиться итератору var_name 
                         * будет приводить к ошибке.
                         */

                        m_scopes.back().dependent.emplace(var_name, depend_name);
                        m_scopes.back().blocker.emplace(depend_name, dre->getLocation());

                        llvm::outs() << dre->getLocation().printToString(m_CI.getSourceManager()) << "\n";

                        depend_name += "=>";
                        depend_name += var_name;

                        FIXIT_DIAG(var->getLocation(), "Depended variables", "depend", depend_name);
                    }
                }
            }
            return true;
        }

        bool checkBlockerMethod(const std::string_view name, const DeclRefExpr & ref) {
            //            ref.IgnoreUnlessSpelledInSource()->dumpColor();
//
//            llvm::outs() << "checkBlockerMethod - getReferencedDeclOfCallee\n";
//            ref.getReferencedDeclOfCallee()->dumpColor();
//            return true;

            clang::DynTypedNodeList parents = m_CI.getASTContext().getParents(ref);

            int count = 0;
            while (!parents.empty()) {

                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                } else if (const MemberExpr * member = parents[0].get<MemberExpr>()) {
                    //                    member->dump();
                    return true;
                } else if (const CallExpr * call = parents[0].get<CallExpr>()) {
                    //                    call->dump();
                    return true;
                }
                //                if (parents[0].get<CompoundStmt>()) {
                //                    // CompoundStmt - определение внутри блока кода
                //
                //                    level++;
                //                    goto next_parent;
                //                }
                //
                //next_parent:
                //                ;

                if (count++ > 3) {
                    //                    parents[0].dump(llvm::outs(), m_CI.getASTContext());
                    //                    llvm::outs() << "\n";
                    return true;
                }
                parents = m_CI.getASTContext().getParents(parents[0]);
            }
            //            ref->dumpColor();
            return true;
        }

        //        Decl *Expr::getReferencedDeclOfCallee() {
        //            Expr *CEE = IgnoreParenImpCasts();
        //
        //            while (auto *NTTP = dyn_cast<SubstNonTypeTemplateParmExpr>(CEE))
        //                CEE = NTTP->getReplacement()->IgnoreParenImpCasts();
        //
        //            // If we're calling a dereference, look at the pointer instead.
        //            while (true) {
        //                if (auto *BO = dyn_cast<BinaryOperator>(CEE)) {
        //                    if (BO->isPtrMemOp()) {
        //                        CEE = BO->getRHS()->IgnoreParenImpCasts();
        //                        continue;
        //                    }
        //                } else if (auto *UO = dyn_cast<UnaryOperator>(CEE)) {
        //                    if (UO->getOpcode() == UO_Deref || UO->getOpcode() == UO_AddrOf ||
        //                            UO->getOpcode() == UO_Plus) {
        //                        CEE = UO->getSubExpr()->IgnoreParenImpCasts();
        //                        continue;
        //                    }
        //                }
        //                break;
        //            }
        //
        //            if (auto *DRE = dyn_cast<DeclRefExpr>(CEE))
        //                return DRE->getDecl();
        //            if (auto *ME = dyn_cast<MemberExpr>(CEE))
        //                return ME->getMemberDecl();
        //            if (auto *BE = dyn_cast<BlockExpr>(CEE))
        //                return BE->getBlockDecl();
        //
        //            return nullptr;
        //        }

        bool VisitCallExpr(const CallExpr *call) {
            if (isEnabled()) {
                llvm::outs() << "CallExpr: " << call->getDirectCallee()-> getQualifiedNameAsString() << "\n";

                for (unsigned i = 0; i < call->getNumArgs(); i++) {
                    //                    if (call->getArg(i)->getReferencedDeclOfCallee()) {
                    //                        llvm::outs() << "getReferencedDeclOfCallee\n";
                    //                        call->getArg(i)->getReferencedDeclOfCallee()->dumpColor();
                    //                    } else {
                    call->getArg(i)->dumpColor();
                    //                    }
                    //            llvm::outs() << call->dump()"call.getArg(...): ";

                }


                // CallExpr
                // CXXMemberCallExpr

                //                call->dumpColor();
            }
            return true;
        }

        bool VisitDeclRefExpr(const DeclRefExpr * ref) {
            if (isEnabled() && !ref->getDecl()->isFunctionOrFunctionTemplate()) {

                std::string ref_name = ref->getDecl()->getNameAsString();

                llvm::outs() << "DeclRefExpr: " << ref_name << "\n";

                SourceLocation cur_location = ref->getLocation();

                clang::Expr::Classification cls = ref->ClassifyModifiable(m_CI.getASTContext(), cur_location);
                //                Expr::isModifiableLvalueResult mod = ref->isModifiableLvalue(m_CI.getSourceManager(), ref->getLocation());
                llvm::outs() << "isModifiable(): " << cls.getModifiable() << "\n";


                auto found = findBlocker(ref_name);
                if (found != ScopeLevel::BlockerEnd) {
                    if (found->second == cur_location) {

                        // First use -> clear source location
                        llvm::outs() << "Clear location!\n";
                        found->second = SourceLocation();

                    } else {

                        if (checkBlockerMethod(ref_name, *ref)) {
                            found->second = cur_location;
                            llvm::outs() << "Set location: " << cur_location.printToString(m_CI.getSourceManager()) << "\n";
                        }
                    }
                }

                auto depend_found = findDependent(ref_name);
                if (depend_found != ScopeLevel::DependentEnd) {

                    llvm::outs() << "Depend found: " << ref_name << "\n";

                    auto block_found = findBlocker(depend_found->second);
                    if (block_found != ScopeLevel::BlockerEnd) {

                        llvm::outs() << "Block found: " << depend_found->second << "\n";

                        if (block_found->second.isValid()) {
                            FIXIT_ERROR(ref->getLocation(), "Using the dependent variable after changing the main variable!", "error", block_found->second.printToString(m_CI.getSourceManager()));
                        } else {
                            llvm::outs() << "Depended " << ref_name << " corrected!\n";
                        }
                    }
                }

            }
            return true;
        }

        bool checkCallExpr(const CallExpr & call) {


            //CXXMemberCallExpr 0x5d4170f92650 'void'
            //`-MemberExpr 0x5d4170f924d0 '<bound member function type>' .clear 0x73b43e7ce7e0
            //  `-DeclRefExpr 0x5d4170f924b0 'std::vector<int>':'class std::vector<int>' lvalue Var 0x73b43dfc9d90 'vect' 'std::vector<int>':'class std::vector<int>'

            //CallExpr 0x73b43df4b9c8 'void'
            //|-ImplicitCastExpr 0x73b43df4b9b0 'void (*)(class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >, class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >)' <FunctionToPointerDecay>
            //| `-DeclRefExpr 0x5d4170f939e0 'void (class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >, class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >)' lvalue Function 0x5d4170f938d8 'sort' 'void (class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >, class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >)' (FunctionTemplate 0x5d416f53f4e8 'sort')
            //|   `-NestedNameSpecifier Namespace 0x73b43e28b4a0 'std'
            //|-CXXConstructExpr 0x73b43df4bfa0 'class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >' 'void (const __normal_iterator<int *, vector<int> > &) noexcept'
            //| `-ImplicitCastExpr 0x73b43df4bf50 'const __normal_iterator<int *, vector<int> >':'const class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >' lvalue <NoOp>
            //|   `-DeclRefExpr 0x5d4170f926f0 'iterator':'class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >' lvalue Var 0x73b43e7ebc58 'beg' 'iterator':'class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >'
            //`-CallExpr 0x5d4170f93678 'decltype(__cont.end())':'class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >'
            //  |-ImplicitCastExpr 0x5d4170f93660 'auto (*)(class std::vector<int> &) -> decltype(__cont.end())' <FunctionToPointerDecay>
            //  | `-DeclRefExpr 0x5d4170f932c0 'auto (class std::vector<int> &) -> decltype(__cont.end())' lvalue Function 0x5d4170f92b88 'end' 'auto (class std::vector<int> &) -> decltype(__cont.end())' (FunctionTemplate 0x5d416f0bfb78 'end')
            //  |   `-NestedNameSpecifier Namespace 0x73b43e28b4a0 'std'
            //  `-DeclRefExpr 0x5d4170f927a0 'std::vector<int>':'class std::vector<int>' lvalue Var 0x73b43dfc9d90 'vect' 'std::vector<int>':'class std::vector<int>'
            //CallExpr 0x5d4170f93678 'decltype(__cont.end())':'class __gnu_cxx::__normal_iterator<int *, class std::vector<int> >'
            //|-ImplicitCastExpr 0x5d4170f93660 'auto (*)(class std::vector<int> &) -> decltype(__cont.end())' <FunctionToPointerDecay>
            //| `-DeclRefExpr 0x5d4170f932c0 'auto (class std::vector<int> &) -> decltype(__cont.end())' lvalue Function 0x5d4170f92b88 'end' 'auto (class std::vector<int> &) -> decltype(__cont.end())' (FunctionTemplate 0x5d416f0bfb78 'end')
            //|   `-NestedNameSpecifier Namespace 0x73b43e28b4a0 'std'
            //`-DeclRefExpr 0x5d4170f927a0 'std::vector<int>':'class std::vector<int>' lvalue Var 0x73b43dfc9d90 'vect' 'std::vector<int>':'class std::vector<int>'
            //

            //            call.dump();

            //            for (unsigned i = 0; i < call.getNumArgs(); i++) {
            //                llvm::outs() << "call.getArg(...): ";
            //                call.getArg(i)->dumpColor();
            //
            //                if (const DeclRefExpr * decl_expr = dyn_cast_or_null<DeclRefExpr>(call.getArg(i)->IgnoreUnlessSpelledInSource())) {
            //                    std::string var_name = decl_expr->getNameInfo().getAsString();
            //                    auto found = m_iter_depend.find(var_name);
            //                    if (found != m_iter_depend.end()) {
            //                        auto stoper = m_iter_stoper.find(found->second);
            //                        if (stoper == m_iter_stoper.end()) {
            //                            FIXIT_ERROR(decl_expr->getLocation(), "Logic error!", "error", var_name);
            //                        } else if (stoper->second.isValid()) {
            //                            FIXIT_ERROR(stoper->second, "Using the dependent variable after changing the main variable!", "error", var_name);
            //                        }
            //                    }
            //                }
            //            }


            if (call.getNumArgs() == 2) {

                if (call.getDirectCallee()->getNameInfo().getAsString() .compare("swap") != 0) {
                    return true;
                }

                const DeclRefExpr * left;
                VAR_LIST_INVAL var_left;
                const char * found_left = expandAssignArgs(call.getArg(0), left, var_left);


                const DeclRefExpr * right;
                VAR_LIST_INVAL var_right;
                const char * found_right = expandAssignArgs(call.getArg(1), right, var_right);


                if (found_left && found_right
                        && SHARED == found_left && SHARED == found_right) {

                    if (var_left->second.level != var_right->second.level) {

                        FIXIT_ERROR(call.getExprLoc(), "Cannot swap a shared variables of different lexical levels", "error");
                        return true;
                    }

                    FIXIT_DIAG(call.getExprLoc(), "Swap shared variables of the same lexical level", "approved");

                } else {

                    FIXIT_DIAG(call.getExprLoc(), "Swap not share", "approved");
                }
            }
            return true;
        }

        bool checkCXXOperatorCallExpr(const CXXOperatorCallExpr & assign) {

            if (assign.getNumArgs() == 2) {


                const DeclRefExpr * left;
                VAR_LIST_INVAL var_left;
                const char * found_left = expandAssignArgs(assign.getArg(0), left, var_left);

                const DeclRefExpr * right;
                VAR_LIST_INVAL var_right;
                const char * found_right = expandAssignArgs(assign.getArg(1), right, var_right);


                if (found_left == SHARED && found_right == SHARED) {

                    if (var_left->second.level >= var_right->second.level) {

                        FIXIT_ERROR(assign.getOperatorLoc(), "Cannot copy a shared variable to an equal or higher lexical level", "error");
                        return true;
                    }

                    FIXIT_DIAG(assign.getOperatorLoc(), "Copy share variable", "approved");

                    //                    //                        llvm::outs() << "expr_right->IgnoreUnlessSpelledInSource()->dumpColor();\n";
                    //                    //                        expr_right->IgnoreUnlessSpelledInSource()->dumpColor();
                    //                    //                        llvm::outs() << "expr_right->IgnoreUnlessSpelledInSource()->dumpColor();\n";
                    //                    //                        expr_right->dumpColor();
                    //
                } else {

                    //                    if (assign.isAssignmentOp()) {
                    //                        auto found = m_iter_stoper.find(left->getNameInfo().getAsString());
                    //                        if (found != m_iter_stoper.end()) {
                    //                            found->second = assign.getOperatorLoc();
                    //                        }
                    //                    }

                    FIXIT_DIAG(assign.getOperatorLoc(), "Assign not share", "approved");
                }
            }
            return true;
        }

        bool checkCXXRecordDecl(const CXXRecordDecl & base) {
            if (AnnotateAttr * attr = base.getAttr<AnnotateAttr>()) {
                if (attr->getAnnotation() != "memsafe") {
                    return true;
                }

                //  Mark only
                checkUnsafe(base);

                std::string name = base.getQualifiedNameAsString();

                //bool 	isInAnonymousNamespace () const
                if (name.find("(anonymous namespace)") != std::string::npos) {

                    getDiag().Report(base.getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Error,
                            "Define classes in anonymous namespaces are not supported yet!"));
                    return false;
                }


                static std::string list;
                if (list.empty()) {
                    for (auto &elem : attArgs) {

                        if (!list.empty()) {
                            list += "', '";
                        }
                        list += elem;
                    }
                    list.insert(0, "'");
                    list += "'";
                }

                std::string first_attr;

                if (attr->args_size() >= 1) {
                    if (StringLiteral * str = dyn_cast_or_null<StringLiteral>(*attr->args_begin())) {
                        first_attr = str->getString().str();
                    } else {
                        getDiag().Report(base.getLocation(), getDiag().getCustomDiagID(
                                DiagnosticsEngine::Error, "First argument should be a string literal"));
                    }
                } else {

                    getDiag().Report(base.getLocation(), getDiag().getCustomDiagID(
                            DiagnosticsEngine::Error, "Expects string argument"));
                }

                std::set<std::string>::iterator found = attArgs.end();

                //                if (first_attr.compare(defineArg) == 0) {
                //                    found = attArgs.find(first_attr);
                //                } else if (attr->args_size() == 2) {
                //                }
                //
                //                if (found == attArgs.end()) {
                //                    clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(base.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                //                            DiagnosticsEngine::Error,
                //                            "Unknown argument '%0'. Expected string argument from the following list: %1"));
                //                    DB.AddString(str->getString().str());
                //                    DB.AddString(list);
                //                }
                //
                //                if (StringLiteral * str = dyn_cast_or_null<StringLiteral>(*attr->args_begin())) {
                //
                //                    auto found = attArgs.find(str->getString().str());
                //                    if (found == attArgs.end()) {
                //                        clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(base.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                //                                DiagnosticsEngine::Error,
                //                                "Unknown argument '%0'. Expected string argument from the following list: %1"));
                //                        DB.AddString(str->getString().str());
                //                        DB.AddString(list);
                //                    }
                //
                //                    appendDecl(base, str->getString().str());
                //
                //                }

            }

            return true;
        }

        std::string GetFullTypeName(ASTContext &Ctx, QualType QT) {
            PrintingPolicy Policy(Ctx.getPrintingPolicy());
            Policy.SuppressScope = true;
            Policy.AnonymousTagLocations = true;

            return QT.getAsString(Policy);
        }

        const Expr * removeTempExpr(const Expr * expr) {
            if (expr && (isa<ImplicitCastExpr>(expr) || isa<ExprWithCleanups>(expr) || isa<CXXBindTemporaryExpr>(expr))) {

                return expr->IgnoreUnlessSpelledInSource();
            }
            return expr;
        }

        const Expr * getExprInitializer(const VarDecl & var) {
            //    CXXConstructExpr
            //    ImplicitCastExpr
            //            CXXMemberCallExpr
            //    ExprWithCleanups
            //            CXXBindTemporaryExpr
            //                    CXXMemberCallExpr

            const Expr * result = removeTempExpr(var.getAnyInitializer());

            if (!result || isa<StringLiteral>(result) || isa<IntegerLiteral>(result)) {
                return nullptr;
            }

            if (isa<CXXMemberCallExpr>(result) || isa<CXXConstructExpr>(result) || isa<UnaryOperator>(result)) {
                return result;
            }

            FIXIT_DIAG(var.getLocation(), "Unknown VarDecl initializer", "unknown");
            var.getAnyInitializer()->dumpColor();

            return nullptr;

        }

        bool checkVarDecl(const VarDecl * var) {

            if (!var) {
                return false;
            }


            //VarDecl 0x7dca4a5eb838 <_example.cpp:13:13, col:35> col:18 used beg 'iterator':'__gnu_cxx::__normal_iterator<int *, std::vector<int>>' cinit
            //`-CXXMemberCallExpr 0x7dca4a5f1700 <col:24, col:35> 'iterator':'__gnu_cxx::__normal_iterator<int *, std::vector<int>>'
            //  `-MemberExpr 0x7dca4a5eb920 <col:24, col:29> '<bound member function type>' .begin 0x7dca4a5c86e0
            //    `-DeclRefExpr 0x7dca4a5eb8a0 <col:24> 'std::vector<int>' lvalue Var 0x7dca49e02940 'vect' 'std::vector<int>'            

            //VarDecl 0x7cdce3767838 <_example.cpp:13:13, col:41> col:24 used beg 'const iterator':'const __gnu_cxx::__normal_iterator<int *, std::vector<int>>' cinit
            //`-ImplicitCastExpr 0x7cdce376d810 <col:30, col:41> 'const iterator':'const __gnu_cxx::__normal_iterator<int *, std::vector<int>>' <NoOp>
            //  `-CXXMemberCallExpr 0x7cdce376d700 <col:30, col:41> 'iterator':'__gnu_cxx::__normal_iterator<int *, std::vector<int>>'
            //    `-MemberExpr 0x7cdce3767920 <col:30, col:35> '<bound member function type>' .begin 0x7cdce37446e0
            //      `-DeclRefExpr 0x7cdce37678a0 <col:30> 'std::vector<int>' lvalue Var 0x7cdce2f14940 'vect' 'std::vector<int>'

            //VarDecl 0x717227bfcd68 <_example.cpp:15:13, col:44> col:24 end_const 'const reverse_iterator':'const std::reverse_iterator<__gnu_cxx::__normal_iterator<int *, std::vector<int>>>' callinit
            //`-ImplicitCastExpr 0x7172271ef1a0 <col:34, col:44> 'const reverse_iterator':'const std::reverse_iterator<__gnu_cxx::__normal_iterator<int *, std::vector<int>>>' <NoOp>
            //  `-CXXMemberCallExpr 0x7172271ef078 <col:34, col:44> 'reverse_iterator':'std::reverse_iterator<__gnu_cxx::__normal_iterator<int *, std::vector<int>>>'
            //    `-MemberExpr 0x717227bfce50 <col:34, col:39> '<bound member function type>' .rend 0x717227bc9580
            //      `-DeclRefExpr 0x717227bfcdd0 <col:34> 'std::vector<int>' lvalue Var 0x717227434d90 'vect' 'std::vector<int>'



            const Type * type_ptr = var->getType().getTypePtrOrNull();

            const Expr * init = getExprInitializer(*var);
            if (init) {
                type_ptr = init->getType().getTypePtrOrNull();
            } else {
                type_ptr = var->getType().getTypePtrOrNull();
            }
            std::string type_name = getCXXTypeName(type_ptr);

            if (type_ptr) {
                type_name = getCXXTypeName(type_ptr);
                //
                //                if (const CXXRecordDecl * decl = type_ptr->getAsCXXRecordDecl()) { // Only C++ classes are checked
                //
                //                    llvm::raw_string_ostream name(type_name);
                //                    decl->printQualifiedName(name);
                //                }

            } else {
                FIXIT_DIAG(var->getLocation(), "Unknown VarDecl type", "unknown");
                var->dumpColor();
            }


            std::string var_name = var->getNameAsString();
            const char * found_type = checkClassName(type_name);

            if (!found_type) {
                m_varOther.emplace(var_name);
                return true;
            }


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
                //            } else if (INVAL == found_type) {
                //
                //                FIXIT_DIAG(var->getLocation(), "Iterator found", "approved");
                //
                //                if (const CXXMemberCallExpr * calle = dyn_cast_or_null<CXXMemberCallExpr>(init)) {
                //
                //                    calle->getImplicitObjectArgument()->dumpColor();
                //
                //                    if (const DeclRefExpr * dre = dyn_cast<DeclRefExpr>(calle->getImplicitObjectArgument()->IgnoreUnlessSpelledInSource())) {
                //
                //                        std::string decl_name = dre->getDecl()->getNameAsString();
                //
                //                        /* var_name - итератор объекта из переменной decl_name
                //                         * итератор можно использовать пока не будет изменена исходная переменная
                //                         * или вызов не константного метода.
                //                         * 
                //                         * Первое изменение к decl_name устанавливает SourceLocation,
                //                         * и после этого любая попытка обратиться итератору var_name 
                //                         * будет приводить к ошибке.
                //                         */
                //
                //                        m_iter_depend.emplace(var_name, decl_name);
                //                        m_iter_stoper.emplace(decl_name, SourceLocation());
                //
                //                        decl_name += " >> ";
                //                        decl_name += var_name;
                //
                //                        FIXIT_DIAG(var->getLocation(), "Depended variables", "depend", decl_name);
                //                    }
                //                }

            } else {

                FIXIT_DIAG(var->getLocation(), "Variable found", "approved");

                VarInfo::DeclType parent_name = std::monostate();
                int level = 0;

                parent_name = findParenName(m_CI.getASTContext().getParents(*var), level);

                AddVariable(*var, found_type, level, parent_name);
            }

            //            llvm::outs() << type_name << "  " << var_name << "\n\n\n";
            //            var->dumpColor();

            return true;

            //            //hasAutomaticStorageDuration()
            //            //hasGlobalStorage()
            //            //hasThreadStorageDuration()
            //
            //
            //            //            // Mark only                
            //            //            checkUnsafe(*var);
            //
            //            VarInfo::DeclType parent_name = std::monostate();
            //            int level = 0;
            //
            //            // llvm::outs() << "\nclang::DynTypedNodeList NodeList = Result.Context->getParents(*var)\n";
            //
            //
            //
            //            parent_name = findParenName(Instance.getASTContext().getParents(*var), level);
            //            if (var->getStorageDuration() == SD_Static) {
            //                if (std::string_view(MemSafeOptions::AUTO).compare(found_type) == 0) {
            //                    FIXIT_ERROR(var->getLocation(), "Create auto variabe as static", "error");
            //                }
            //            }
            //
            //            //            AddVariable(*var, found_type, level, parent_name);
            //            //            FIXIT_DIAG(var->getLocation(), "Create variable", found_type, level, parent_name);
            //
            return true;
        }


        //        std::vector<int> vect;
        //        auto iter = vect.begin();
        //        auto refff = &vect;        

        //VarDecl 0x5b081865fa60 </home/rsashka/SOURCE/memsafe/_example.cpp:16:9, col:26> col:26 used vect 'std::vector<int>' callinit destroyed
        //`-CXXConstructExpr 0x5b081868bb18 <col:26> 'std::vector<int>' 'void () noexcept'
        //
        //VarDecl 0x5b081868bbb0 </home/rsashka/SOURCE/memsafe/_example.cpp:17:9, col:32> col:14 iter 'iterator':'__gnu_cxx::__normal_iterator<int *, std::vector<int>>' cinit
        //`-CXXMemberCallExpr 0x5b081868bcc8 <col:21, col:32> 'iterator':'__gnu_cxx::__normal_iterator<int *, std::vector<int>>'
        //  `-MemberExpr 0x5b081868bc98 <col:21, col:26> '<bound member function type>' .begin 0x5b0818678f38
        //    `-DeclRefExpr 0x5b081868bc18 <col:21> 'std::vector<int>' lvalue Var 0x5b081865fa60 'vect' 'std::vector<int>'
        //
        //
        //VarDecl 0x5b081868f530 </home/rsashka/SOURCE/memsafe/_example.cpp:18:9, col:23> col:14 refff 'std::vector<int> *' cinit
        //`-UnaryOperator 0x5b081868f5b8 <col:22, col:23> 'std::vector<int> *' prefix '&' cannot overflow
        //  `-DeclRefExpr 0x5b081868f598 <col:23> 'std::vector<int>' lvalue Var 0x5b081865fa60 'vect' 'std::vector<int>'

        bool checkReturnStmt(const ReturnStmt & ret) {


            if (const ExprWithCleanups * inplace = dyn_cast_or_null<ExprWithCleanups>(ret.getRetValue())) {
                FIXIT_DIAG(inplace->getBeginLoc(), "Return inplace", "approved");
                return true;
            }

            if (const CXXConstructExpr * expr = dyn_cast_or_null<CXXConstructExpr>(ret.getRetValue())) {


                const char * found_type = checkClassName(expr->getType().getAsString());
                if (!found_type) {
                    return true;
                }

                if (std::string_view(AUTO).compare(found_type) == 0) {
                    FIXIT_ERROR(ret.getRetValue()->getExprLoc(), "Return auto type", "error");
                    return true;
                } else if (std::string_view(SHARED).compare(found_type) == 0) {
                    FIXIT_ERROR(ret.getRetValue()->getExprLoc(), "Return share type", "error");

                    return true;
                }
            }
            return true;
        }

        bool checkUnaryOperator(const UnaryOperator * unary) {
            if (!unary) {
                return false;
            }


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

                FIXIT_ERROR(unary->getOperatorLoc(), "Operator for address arithmetic", "error");
                //                unary->dump();
            }

            return true;
        }

        //        ScopeLevel parseParmVarDecl(const FunctionDecl &func) {
        //            ScopeLevel result({
        //                {}, &func
        //            });
        //
        //            for (unsigned i = 0; i < func.getNumParams(); i++) {
        //                const ParmVarDecl * param = func.getParamDecl(i);
        //                assert(param);
        //                param->dump();
        //                const clang::QualType type = param->getType();
        //                result.vars.emplace(param->getQualifiedNameAsString(), DeclInfo{
        //                    param, ""
        //                });
        //
        //            }
        //            return result;
        //        }
        /*
         * 
         * 
         * 
         * 
         * 
         * 
         *  
         */

        //VarDecl 0x5de70866d140 </home/rsashka/SOURCE/memsafe/_example.cpp:15:9, col:23> col:14 invalid refff 'auto' cinit
        //`-UnaryOperator 0x5de70866d1c8 <col:22, col:23> '<dependent type>' contains-errors prefix '&' cannot overflow
        //  `-RecoveryExpr 0x5de70866d1a8 <col:23> '<dependent type>' contains-errors lvalue

        bool TraverseDecl(Decl * D) {

            if (checkDeclAttributes(dyn_cast_or_null<EmptyDecl>(D))) {

            } else if (const NamespaceDecl * ns = dyn_cast_or_null<NamespaceDecl>(D)) {

                checkDeclAttributes(ns);

            } else if (const FunctionDecl * func = dyn_cast_or_null<FunctionDecl>(D)) {

                if (isEnabled()) {

                    llvm::outs() << "Enter FunctionDecl\n";

                    //                    D->dumpColor();

                    //                m_scopes.push_back(parseParmVarDecl(*func));

                    m_scopes.push_back(ScopeLevel(func));
                    //                checkFunctionDecl(*func);
                    //parseParmVarDecl

                    RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);

                    //                m_scopes.pop_back();
                    m_scopes.pop_back();
                    llvm::outs() << "Exit FunctionDecl\n";
                    return true;
                }

            }


            if (isEnabled()) {

                if (const VarDecl * decl = dyn_cast_or_null<VarDecl>(D)) {

                    m_decl.push_back(decl);

                    checkVarDecl(decl);

                    RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);

                    m_decl.pop_back();
                    return true;

                } else if (const CXXRecordDecl * base = dyn_cast_or_null<CXXRecordDecl>(D)) {
                    checkCXXRecordDecl(*base);
                }

            }


            //            if (D && opts->isEnabled()) {
            //                D->dumpColor();
            //            }

            RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);

            return true;
        }

        bool TraverseStmt(Stmt * stmt) {
            if (isEnabledStatus()) {
                // If the plugin does not start to be defined, we do not process or check anything else.

                //                if (stmt) {
                //                    stmt->dumpColor();
                //                }

                if (const CompoundStmt * block = dyn_cast_or_null<CompoundStmt>(stmt)) {

                    m_level.push_back(block);
                    m_scopes.push_back(ScopeLevel());
                    //                checkFunctionDecl(*func);
                    //ParmVarDecl

                    RecursiveASTVisitor<MemSafePlugin>::TraverseStmt(stmt);

                    m_scopes.pop_back();
                    m_level.pop_back();
                    return true;

                } else if (checkUnaryOperator(dyn_cast_or_null<UnaryOperator>(stmt))) {
                }

                if (const CXXOperatorCallExpr * assign = dyn_cast_or_null<CXXOperatorCallExpr>(stmt)) {
                    checkCXXOperatorCallExpr(*assign);
                } else if (const CallExpr * call = dyn_cast_or_null<CallExpr>(stmt)) {
                    checkCallExpr(*call);
                } else if (const ReturnStmt * ret = dyn_cast_or_null<ReturnStmt>(stmt)) {

                    checkReturnStmt(*ret);
                    //                } else if (stmt) {
                    //                    // stmt->dumpColor();
                }

                RecursiveASTVisitor<MemSafePlugin>::TraverseStmt(stmt);
            }
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

            if (Rewriter) {

                Rewriter->WriteFixedFiles();
            }

            llvm::outs() << "\n\n\ndump();\n";
            plugin->dump();

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
                            llvm::errs() << "To enable FixIt output to a file, you must specify the extension of the file being created!";
                            return false;
                        }
                        if (fixit_file_ext[0] != '.') {
                            fixit_file_ext.insert(0, ".");
                        }
                        llvm::outs() << "\033[1;46;34mEnabled FixIt output to a file with the extension: '" << fixit_file_ext << "'\033[0m\n";
                    } else if (first.compare("marker") == 0) {
                        if (second.empty()) {
                            llvm::errs() << "Debug trace marker cannot be empty!";
                            return false;
                        }
                        m_marker = second;
                        llvm::outs() << "\033[1;46;34mUsing debug trace marker: '" << fixit_file_ext << "'\033[0m\n";
                    } else {
                        llvm::errs() << "Unknown plugin argument: '" << elem << "'!";
                        return false;
                    }
                }
            }

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A("memsafe", "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<MemSafePluginASTAction> S("memsafe", "Memory safety plugin");


