#include <iostream>

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


    //    static std::string memsafeBase = "memsafe::Variable";
    //    static SourceLocation memsafeLoc;
    //    static std::set<std::string> msBaseList;

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

            if (const CXXRecordDecl * rec = dyn_cast_or_null<CXXRecordDecl>(D)) {

                return true;

            } else if (const NamespaceDecl * ns = dyn_cast_or_null<NamespaceDecl>(D)) {

                return true;

            }

            return false;
        }

        AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &Attr) const override {

            if (Attr.getNumArgs() > 2) {

                unsigned ID = S.getDiagnostics().getCustomDiagID(
                        DiagnosticsEngine::Error,
                        "'memsafe' attribute accept only one argument");
                S.Diag(Attr.getLoc(), ID);
                return AttributeNotApplied;
            }

            //            std::cout << "Attr.getNumArgs() " << Attr.getNumArgs() << "\n";

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

            //            std::cout << "AttributeApplied !!!!\n";
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
            //            std::cout << "diagAppertainsToStmt " << isa<ReturnStmt>(St) << "\n";
            //            std::cout.flush();
            return true; //isa<ReturnStmt>(St);
        }
        //
        //        AttrHandling handleStmtAttribute(Sema &S, Stmt *St, const ParsedAttr &Attr) const override {
        //
        //            std::cout << "handleStmtAttribute\n";
        //            std::cout.flush();
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

        explicit FixItRewriterOptions(const std::string& RewriteSuffix)
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

    class MemSafe {
    protected:
        bool m_enabled;
        bool m_start_define;
    public:
        // The first string arguments in the `memsafe` attribute for working and managing the plugin
        static inline constexpr StringRef defineArg{"define"};
        static inline constexpr StringRef enableArg{"enable"};
        static inline constexpr StringRef disableArg{"disable"};
        static inline constexpr StringRef unsafeArg{"unsafe"};
        static inline constexpr StringRef lineArg{"line"};

        // A list of attribute parameters that the memory safety model will work with, 
        // as well as the names of the class templates that are used to implement each data type.
        //     
        // Template class names are missing until they are defined during plugin initialization 
        // in a namespace with the memsafe attribute and the 'define' argument 
        // (i.e. namespace [[memsafa("define")]] ... )

        static inline const char * VALUE = "value";
        static inline const char * SHARED = "shared";
        static inline const char * WEAK = "weak";
        static inline const char * AUTO = "auto";

        static inline const std::set<std::string> attArgs{VALUE, SHARED, WEAK, AUTO};


        std::map<std::string, std::string> clsUse;

        typedef std::multimap<std::string, VarInfo> VAR_LIST_TYPE;
        typedef std::multimap<std::string, VarInfo>::iterator VAR_LIST_ITER;
        VAR_LIST_TYPE varList;

        std::set<const NamespaceDecl *> unsafeSet;

        MemSafe() : m_enabled(false), m_start_define(false) {

        }

        /**
         * Add variable name to watchlist
         */
        void AddVariable(const VarDecl &var, const char * const &type, const int level, VarInfo::DeclType parent = std::monostate()) {

            if (varList.find(var.getQualifiedNameAsString()) != varList.end()) {
                //  std::cout << "AddVariable dublicate: " << name << "  " << type << " !!!\n";
            }
            varList.emplace(var.getNameAsString(), VarInfo{var, type, level, parent});
            varList.emplace(var.getQualifiedNameAsString(), VarInfo{var, type, level, parent});
        }

        /**
         * Add pointer to unsafe namespace
         */
        void AddUnsafe(const NamespaceDecl *ns) {
            assert(ns);
            unsafeSet.insert(ns);
        }

        /**
         * Check if class name is in the list of monitored classes
         */

        inline const char * checkClassName(std::string_view type) {
            type.remove_suffix(type.size() - type.find("<") - 1);
            auto found = clsUse.find(std::string(type.begin(), type.end()));
            if (found == clsUse.end()) {
                return nullptr;
            }
            return found->second.c_str();
        }

        inline bool isEnabled() {
            return m_enabled;
        }

        inline bool isStartDefine() {
            return m_start_define;
        }

        inline bool startDefine() {
            if (m_start_define || m_enabled) {
                return false;
            }
            m_start_define = true;
            return true;
        }

        bool setEnabled(bool mode, std::string *message = nullptr) {
            if (!mode) {
                m_enabled = false;
                return true;
            }
            bool done = true;
            for (auto &attr : attArgs) {
                for (auto &elem : clsUse) {
                    if (attr.compare(elem.second) == 0) {
                        goto label_done;
                    }
                }
                done = false;

                if (message) {
                    if (!message->empty()) {
                        *message += ", ";
                    }
                    *message += "\"";
                    *message += attr;
                    *message += "\"";
                }
label_done:
                ;
            }
            if (message) {
                message->insert(0, " Template class for [[memsafe(...)]] attribute: ");
                *message += " not defined!";
            }
            m_enabled = done;
            return done;
        }

    };

    /*
     * The problem with the typical approach to AST parsing using visitor and matcher is the following.
     * Both methods focus on finding individual nodes based on predefined conditions,
     * making it difficult to apply some search conditions because of their interrelationships.
     *
     * It would be possible to implement a single anyOf(decl(),stmt()) matcher, but this is not possible
     * because of different return types (the input value has an unresolved overloaded type: Matcher<Decl>&Matcher<Stmt>).
     *
     * The solution is to implement your own RecursiveASTVisitor,
     * where the pass applies the search conditions to each node sequentially,
     * taking into account the current state of the object and is independent of the type of the node being parsed
     * (and this is cleaner than manually getting Stmt using getBody in Decl for Matchers).
     */

    class MemSafePlugin : public RecursiveASTVisitor<MemSafePlugin>, protected MemSafe {
    protected:
        CompilerInstance &Instance;
    public:

        bool is_fixit;
        int64_t line_base;
        int64_t line_number;

        MemSafePlugin(CompilerInstance &I) : Instance(I), is_fixit(false), line_base(0), line_number(0) {
        }

#define FIXIT_DIAG(location, remark, type, ... ) \
            if(is_fixit){ Instance.getDiagnostics().Report(location, Instance.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Remark, remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type, Instance.getSourceManager().getSpellingLineNumber(location) __VA_OPT__(,) __VA_ARGS__)));}

#define FIXIT_ERROR(location, remark, type, ... ) \
            Instance.getDiagnostics().Report(location, Instance.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type, Instance.getSourceManager().getSpellingLineNumber(location) __VA_OPT__(,) __VA_ARGS__)))

        std::string MakeFixitMessage(const std::string_view type, int64_t num, int level = VarInfo::NOLEVEL, const VarInfo::DeclType parent = std::monostate()) {
            std::string message("/* [[memsafe(");
            message += type.begin();
            message += "";
            message += ", ";
            message += std::to_string(num - line_base + line_number);

            if (level != VarInfo::NOLEVEL) {
                message += ", ";
                message += std::to_string(level);
            }

            if (const FunctionDecl * const * func = std::get_if<const FunctionDecl*> (&parent)) {

                message += ", \"";
                message += (*func)->getReturnType().getAsString();
                message += " ";
                message += (*func)->getQualifiedNameAsString();
                message += "(";
                for (int i = 0; i < (*func)->getNumParams(); i++) {
                    if ((*func)->getNumParams() > 1) {
                        message += ", ";
                    }
                    message += (*func)->parameters()[i]->getOriginalType().getAsString();
                    message += " ";
                    message += (*func)->parameters()[i]->getQualifiedNameAsString();
                }
                message += ")\"";

            } else if (const FieldDecl * const * field = std::get_if<const FieldDecl *> (&parent)) {
                message += ", \"class ";
                message += (*field)->getNameAsString();
                message += "\"";
            }

            message += ")]] */ ";

            return message;
        }

        /*
         * 
         * 
         * 
         */

        bool checkNamespaceDecl(const NamespaceDecl &ns) {
            // Check namespace annotation attribute
            // This check should be done first as it is used to enable and disable the plugin.

            if (AnnotateAttr * attr = ns.getAttr<AnnotateAttr>()) {
                if (attr->getAnnotation() == "memsafe") {

                    auto attr_iter = attr->args_begin();

                    StringLiteral *attr_arg = nullptr;
                    if (attr->args_size() >= 1) {
                        attr_arg = dyn_cast_or_null<StringLiteral>(*attr_iter);
                    }

                    if (!attr_arg) {
                        Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "The namespace attribute must contain one string argument!"));
                        return false;
                    }


                    if (MemSafe::defineArg.compare(attr_arg->getString()) == 0) {

                        if (isEnabled()) {
                            Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "You cannot define plugin classes while it is running!"));
                            return false;
                        }


                        if (isStartDefine()) {
                            Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "Memory safety plugin detection is already running!"));
                            return false;
                        }

                        startDefine();

                    } else if (MemSafe::enableArg.compare(attr_arg->getString()) == 0) {

                        std::string message;
                        if (setEnabled(true, &message)) {
                            Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                    DiagnosticsEngine::Remark,
                                    "Memory safety plugin is enabled!"));
                        } else {
                            clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "Error enabling memory safety plugin!%0"));
                            DB.AddString(message);
                            return false;
                        }

                    } else if (MemSafe::unsafeArg.compare(attr_arg->getString()) == 0) {

                        AddUnsafe(&ns);

                    } else if (MemSafe::disableArg.compare(attr_arg->getString()) == 0) {

                        setEnabled(false);
                        Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                DiagnosticsEngine::Remark,
                                "Memory safety plugin is disabled!"));

                    } else if (MemSafe::lineArg.compare(attr_arg->getString()) == 0) {

                        IntegerLiteral *line_arg = nullptr;
                        if (attr->args_size() >= 2) {
                            attr_iter++;
                            //                            (*attr_iter)->dump();
                            line_arg = dyn_cast_or_null<IntegerLiteral>(*attr_iter);
                        }

                        if (!line_arg) {
                            Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "Line number diagnostic expected numeric second argument!"));
                            return false;
                        }

                        line_base = Instance.getSourceManager().getSpellingLineNumber(line_arg->getLocation());
                        line_number = line_arg->getValue().getSExtValue();

                    } else {

                        clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(ns.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "Unknown attribute argument '%0'!"));
                        DB.AddString(attr_arg->getString());
                    }

                    // ns.dumpColor();
                    return true;
                }
            }
            return false;
        }

        /*
         * 
         * 
         * 
         * 
         */
        bool checkUnsafe(const Decl & decl) {

            clang::DynTypedNodeList parents = Instance.getASTContext().getParents(decl);

            while (!parents.empty()) {
                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                }

                if (const NamespaceDecl * ns = parents[0].get<NamespaceDecl>()) {
                    if (unsafeSet.find(ns) != unsafeSet.end()) {
                        FIXIT_DIAG(decl.getBeginLoc(), "Add mark \"unsafe\" namesapace", "unsafe");

                        return true;
                    }
                }

                parents = Instance.getASTContext().getParents(parents[0]);
            }
            return false;
        }

        void appendDecl(const NamedDecl &decl, const std::string_view attr) {

            std::string name_full = decl.getQualifiedNameAsString();
            std::string name_short = decl.getNameAsString();

            std::cout << "Register template class '" << name_full << "' and '" << name_short << "' with '" << attr << "' attribute!\n";
            std::cout.flush();
            name_full.append("<"); // For complete full template name 
            clsUse[name_full] = attr.begin();

            name_short.append("<"); // For complete full template name 
            clsUse[name_short] = attr.begin();
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

                parents = Instance.getASTContext().getParents(parents[0]);
            }
            return std::monostate();
        }

        /*
         * 
         * 
         */
        const char * expandAssignArgs(const Expr * expr, const DeclRefExpr * &decl, MemSafe::VAR_LIST_ITER & iter) {
            decl = nullptr;
            iter = varList.end();

            if (!expr) {
                return nullptr;
            }

            decl = dyn_cast_or_null<DeclRefExpr>(expr->IgnoreUnlessSpelledInSource());
            if (!decl) {
                return nullptr;
            }

            clang::QualType type;

            const ValueDecl *value = decl->getDecl();

            if (value) {
                type = value->getType();
            } else {
                type = expr->getType();
            }

            const char * name = checkClassName(type.getUnqualifiedType().getAsString());


            //            std::cout << name << "  " << type.getUnqualifiedType().getAsString() << " !!!!!!!!!!!!!!\n\n";

            iter = varList.find(decl->getNameInfo().getAsString());

            if (name) {
                if (iter == varList.end()) {
                    decl->dumpColor();
                    for (auto &elem : varList) {
                        std::cout << elem.first;
                        std::cout << " ";
                    }
                    std::cout << "\n";

                    FIXIT_ERROR(decl->getLocation(), "Variable detected ERROR", "error");
                } else {

                    FIXIT_DIAG(decl->getLocation(), "Variable detected", name, iter->second.level);
                }
            }
            return name;
        }

        bool checkCallExpr(const CallExpr & call) {

            if (call.getNumArgs() == 2) {

                if (call.getDirectCallee()->getNameInfo().getAsString() .compare("swap") != 0) {
                    return true;
                }

                const DeclRefExpr * left;
                MemSafe::VAR_LIST_ITER var_left;
                const char * found_left = expandAssignArgs(call.getArg(0), left, var_left);


                const DeclRefExpr * right;
                MemSafe::VAR_LIST_ITER var_right;
                const char * found_right = expandAssignArgs(call.getArg(1), right, var_right);


                if (found_left && found_right
                        && std::string_view(MemSafe::SHARED).compare(found_left) == 0
                        && std::string_view(MemSafe::SHARED).compare(found_right) == 0) {

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
                MemSafe::VAR_LIST_ITER var_left;
                const char * found_left = expandAssignArgs(assign.getArg(0), left, var_left);

                const DeclRefExpr * right;
                MemSafe::VAR_LIST_ITER var_right;
                const char * found_right = expandAssignArgs(assign.getArg(1), right, var_right);


                if (std::string_view(MemSafe::SHARED).compare(found_left) == 0
                        && std::string_view(MemSafe::SHARED).compare(found_right) == 0) {

                    if (var_left->second.level >= var_right->second.level) {

                        FIXIT_ERROR(assign.getOperatorLoc(), "Cannot copy a shared variable to an equal or higher lexical level", "error");
                        return true;
                    }

                    FIXIT_DIAG(assign.getOperatorLoc(), "Copy share variable", "approved");

                    //                        std::cout << "expr_right->IgnoreUnlessSpelledInSource()->dumpColor();\n";
                    //                        expr_right->IgnoreUnlessSpelledInSource()->dumpColor();
                    //                        std::cout << "expr_right->IgnoreUnlessSpelledInSource()->dumpColor();\n";
                    //                        expr_right->dumpColor();

                } else {

                    FIXIT_DIAG(assign.getOperatorLoc(), "Assign not share", "approved");
                }
            }
            return true;
        }

        bool checkCXXRecordDecl(const CXXRecordDecl &base) {
            if (AnnotateAttr * attr = base.getAttr<AnnotateAttr>()) {
                if (attr->getAnnotation() != "memsafe") {
                    return true;
                }

                //  Mark only
                checkUnsafe(base);

                std::string name = base.getQualifiedNameAsString();

                //bool 	isInAnonymousNamespace () const
                if (name.find("(anonymous namespace)") != std::string::npos) {

                    Instance.getDiagnostics().Report(base.getLocation(), Instance.getDiagnostics().getCustomDiagID(
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

                if (attr->args_size() != 1) {
                    clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(base.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                            DiagnosticsEngine::Error,
                            "Expects one string argument from the following list: %0"));
                    DB.AddString(list);
                    return false;
                }

                if (StringLiteral * str = dyn_cast_or_null<StringLiteral>(*attr->args_begin())) {

                    auto found = attArgs.find(str->getString().str());
                    if (found == attArgs.end()) {
                        clang::DiagnosticBuilder DB = Instance.getDiagnostics().Report(base.getLocation(), Instance.getDiagnostics().getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "Unknown argument '%0'. Expected string argument from the following list: %1"));
                        DB.AddString(str->getString().str());
                        DB.AddString(list);
                    }

                    appendDecl(base, str->getString().str());

                }

            }

            return true;
        }

        bool checkVarDecl(const VarDecl &var) {

            //hasAutomaticStorageDuration()
            //hasGlobalStorage()
            //hasThreadStorageDuration()

            QualType type = var.getType();

            std::string type_name = var.getType().getAsString();
            auto found = clsUse.find(type_name.substr(0, type_name.find('<') + 1));

            const char * found_type = checkClassName(type_name);

            if (!found_type) {
                return true;
            }

            // Mark only                
            checkUnsafe(var);

            VarInfo::DeclType parent_name = std::monostate();
            int level = 0;

            // llvm::outs() << "\nclang::DynTypedNodeList NodeList = Result.Context->getParents(*var)\n";

            parent_name = findParenName(Instance.getASTContext().getParents(var), level);
            if (var.getStorageDuration() == SD_Static) {
                if (std::string_view(MemSafe::AUTO).compare(found_type) == 0) {
                    FIXIT_ERROR(var.getLocation(), "Create auto variabe as static", "error");
                }
            }

            AddVariable(var, found_type, level, parent_name);
            FIXIT_DIAG(var.getLocation(), "Create variable", found_type, level, parent_name);

            return true;
        }

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

                if (std::string_view(MemSafe::AUTO).compare(found_type) == 0) {
                    FIXIT_ERROR(ret.getRetValue()->getExprLoc(), "Return auto type", "error");
                    return true;
                } else if (std::string_view(MemSafe::SHARED).compare(found_type) == 0) {
                    FIXIT_ERROR(ret.getRetValue()->getExprLoc(), "Return share type", "error");
                    return true;
                }
            }
            return true;
        }

        /*
         * 
         * 
         * 
         * 
         * 
         * 
         *  
         */

        bool TraverseDecl(Decl *D) {

            if (const NamespaceDecl * ns = dyn_cast_or_null<NamespaceDecl>(D)) {
                checkNamespaceDecl(*ns);
            } else if (const VarDecl * var = dyn_cast_or_null<VarDecl>(D)) {
                checkVarDecl(*var);
            } else if (const CXXRecordDecl * base = dyn_cast_or_null<CXXRecordDecl>(D)) {
                checkCXXRecordDecl(*base);

            }

            RecursiveASTVisitor<MemSafePlugin>::TraverseDecl(D);
            return true;
        }

        bool TraverseStmt(Stmt * stmt) {
            if (isEnabled()) {
                // If the plugin does not start to be defined, we do not process or check anything else.

                if (const CXXOperatorCallExpr * assign = dyn_cast_or_null<CXXOperatorCallExpr>(stmt)) {
                    checkCXXOperatorCallExpr(*assign);
                } else if (const CallExpr * call = dyn_cast_or_null<CallExpr>(stmt)) {
                    checkCallExpr(*call);
                } else if (const ReturnStmt * ret = dyn_cast_or_null<ReturnStmt>(stmt)) {
                    checkReturnStmt(*ret);
                } else if (stmt) {
                    // stmt->dumpColor();
                }

                RecursiveASTVisitor<MemSafePlugin>::TraverseStmt(stmt);
            }
            return true;
        }
    };

    /*
     * 
     */

    class MemSafePluginASTConsumer : public ASTConsumer {
    public:

        MemSafePlugin plugin;
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

            plugin.is_fixit = !!Rewriter;
            plugin.TraverseDecl(context.getTranslationUnitDecl());


            if (Rewriter) {
                Rewriter->WriteFixedFiles();
            }

        }


    public:

        MemSafePluginASTConsumer(CompilerInstance & Instance) : plugin(Instance), FixItOptions("") {
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
    public:

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {
            std::unique_ptr<MemSafePluginASTConsumer> obj = std::unique_ptr<MemSafePluginASTConsumer>(new MemSafePluginASTConsumer(Compiler));
            obj->FixItOptions.RewriteSuffix = fixit_file_ext;
            return obj;
        }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
            std::cout << std::unitbuf;

            for (auto &elem : args) {
                if (elem.compare("fixit=") > 0) {
                    fixit_file_ext = elem.substr(6);
                    if (fixit_file_ext.empty()) {
                        std::cerr << "To enable FixIt output to a file, you must specify the extension of the file being created!";
                        return false;
                    }
                    if (fixit_file_ext[0] != '.') {
                        fixit_file_ext.insert(0, ".");
                    }
                    std::cout << "\033[1;46;34mEnabled FixIt output to a file with the extension: '" << fixit_file_ext << "'\033[0m\n";
                } else {
                    std::cerr << "Unknown plugin argument: '" << elem << "'!";
                    return false;
                }
            }

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A("memsafe", "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<MemSafePluginASTAction> S("memsafe", "Memory safety plugin");


