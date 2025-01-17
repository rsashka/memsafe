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

                //            } else if (!isa<FunctionDecl>(D)) {
                //
                //                D->dumpColor();
                //
                //                S.Diag(Attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
                //                        << Attr << Attr.isRegularKeywordAttribute() << "functions or method";
                //
                //                return false;
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

        //        bool diagAppertainsToStmt(Sema &S, const ParsedAttr &Attr, const Stmt *St) const override {
        //            St->dumpColor();
        //            std::cout << "diagAppertainsToStmt " << isa<ReturnStmt>(St) << "\n";
        //            std::cout.flush();
        //            return isa<ReturnStmt>(St);
        //        }
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

    private:
        // The suffix appended to rewritten files.
        std::string RewriteSuffix;
    };




    /*
     * 
     * 
     * 
     */


    //    class MemSafeHandler : public MatchFinder::MatchCallback {
    //    private:
    //
    //        CompilerInstance &Instance;
    //        DiagnosticsEngine &Diag;
    //        ASTContext *context;
    //        //        std::vector<ObjCPropertyDecl *> propertyDeclVector;
    //
    //
    //    public:
    //
    //        MemSafeHandler(CompilerInstance &Instance, DiagnosticsEngine &d) : Instance(Instance), Diag(d) {
    //
    //        }
    //
    //        void setContext(ASTContext &context) {
    //
    //            this->context = &context;
    //        }
    //
    //        static bool checkAnnotate(const Decl * decl, std::string_view name) {
    //            if (!decl) {
    //                return false;
    //            }
    //            //            for (auto &attr : decl->getAttrs()) {
    //            //                if (name.compare(attr->getSpelling()) == 0) {
    //            //                    return true;
    //            //                }
    //            //            }
    //
    //            if (AnnotateAttr * attr = decl->getAttr<AnnotateAttr>()) {
    //
    //                return attr->getAnnotation() == name.begin();
    //            }
    //            return false;
    //        }
    //
    //        bool checkUnsafe(ASTContext &ctx, const Decl & decl) {
    //
    //            clang::DynTypedNodeList parents = ctx.getParents(decl);
    //
    //            while (!parents.empty()) {
    //                if (parents[0].get<TranslationUnitDecl>()) {
    //                    break;
    //                }
    //
    //                if (const NamespaceDecl * ns = parents[0].get<NamespaceDecl>()) {
    //                    if (memsafe.unsafeSet.find(ns) != memsafe.unsafeSet.end()) {
    //                        FIXIT_DIAG(decl.getBeginLoc(), "Add mark \"unsafe\" namesapace", "unsafe");
    //
    //                        return true;
    //                    }
    //                }
    //
    //                parents = ctx.getParents(parents[0]);
    //            }
    //            return false;
    //        }
    //
    //        VarInfo::DeclType findParenName(ASTContext &ctx, clang::DynTypedNodeList parents, int & level) {
    //
    //            level++;
    //
    //            while (!parents.empty()) {
    //
    //                if (parents[0].get<TranslationUnitDecl>()) {
    //                    break;
    //                }
    //
    //                if (const FunctionDecl * func = parents[0].get<FunctionDecl>()) {
    //                    return func;
    //                }
    //
    //                if (parents[0].get<CompoundStmt>()) {
    //                    // CompoundStmt - определение внутри блока кода
    //
    //                    level++;
    //                    goto next_parent;
    //                }
    //
    //next_parent:
    //                ;
    //
    //                parents = ctx.getParents(parents[0]);
    //            }
    //            return std::monostate();
    //        }
    //
    //        /*
    //         * Add template class name with specified attribute
    //         */
    //
    //        void appendDecl(const NamedDecl &decl, const std::string_view attr) {
    //
    //            std::string name_full = decl.getQualifiedNameAsString();
    //            std::string name_short = decl.getNameAsString();
    //
    //            std::cout << "Register template class '" << name_full << "' and '" << name_short << "' with '" << attr << "' attribute!\n";
    //            std::cout.flush();
    //            name_full.append("<"); // For complete full template name 
    //            memsafe.clsUse[name_full] = attr.begin();
    //
    //            name_short.append("<"); // For complete full template name 
    //            memsafe.clsUse[name_short] = attr.begin();
    //        }
    //
    //        /*
    //         * 
    //         * 
    //         */
    //
    //        virtual void run(const MatchFinder::MatchResult &Result) {
    //
    //            if (runAnalysis(Diag, Result)) {
    //
    //                //            const auto ID =
    //                //                    DiagnosticsEngine.getCustomDiagID(clang::DiagnosticsEngine::Warning,
    //                //                    "This should probably be a minus");
    //                //
    //                //            DiagnosticsEngine.Report(StartLocation, ID).AddFixItHint(FixIt);
    //
    //            }
    //        }
    //
    //        bool runAnalysis(DiagnosticsEngine &Diag, const MatchFinder::MatchResult &Result) {
    //
    //            //            if (const NamespaceDecl * ns = Result.Nodes.getNodeAs<NamespaceDecl>("ns")) {
    //            //
    //            //                // Check namespace annotation attribute
    //            //                // This check should be done first as it is used to enable and disable the plugin.
    //            //
    //            //                if (AnnotateAttr * attr = ns->getAttr<AnnotateAttr>()) {
    //            //                    if (attr->getAnnotation() == "memsafe") {
    //            //
    //            //                        StringLiteral *attr_arg = nullptr;
    //            //                        if (attr->args_size() == 1) {
    //            //                            attr_arg = dyn_cast_or_null<StringLiteral>(*attr-> args_begin()); //getArgAsExpr(0));
    //            //                        }
    //            //                        if (!attr_arg) {
    //            //                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                    DiagnosticsEngine::Error,
    //            //                                    "The namespace attribute must contain one string argument!"));
    //            //                            return false;
    //            //                        }
    //            //
    //            //
    //            //                        if (MemSafe::defineArg.compare(attr_arg->getString()) == 0) {
    //            //
    //            //                            if (memsafe.isEnabled()) {
    //            //                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                        DiagnosticsEngine::Error,
    //            //                                        "You cannot define plugin classes while it is running!"));
    //            //                                return false;
    //            //                            }
    //            //
    //            //
    //            //                            if (memsafe.isStartDefine()) {
    //            //                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                        DiagnosticsEngine::Error,
    //            //                                        "Memory safety plugin detection is already running!"));
    //            //                                return false;
    //            //                            }
    //            //
    //            //                            memsafe.startDefine();
    //            //                            return true;
    //            //
    //            //                        } else if (MemSafe::enableArg.compare(attr_arg->getString()) == 0) {
    //            //
    //            //                            std::string message;
    //            //                            if (memsafe.setEnabled(true, &message)) {
    //            //                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                        DiagnosticsEngine::Remark,
    //            //                                        "Memory safety plugin is enabled!"));
    //            //                            } else {
    //            //                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                        DiagnosticsEngine::Error,
    //            //                                        "Error enabling memory safety plugin!%0"));
    //            //                                DB.AddString(message);
    //            //                                return false;
    //            //                            }
    //            //                            return true;
    //            //
    //            //                        } else if (MemSafe::unsafeArg.compare(attr_arg->getString()) == 0) {
    //            //
    //            //                            memsafe.AddUnsafe(ns);
    //            //                            return true;
    //            //
    //            //                        } else if (MemSafe::disableArg.compare(attr_arg->getString()) == 0) {
    //            //
    //            //                            memsafe.setEnabled(false);
    //            //                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                    DiagnosticsEngine::Remark,
    //            //                                    "Memory safety plugin is disabled!"));
    //            //                            return true;
    //            //
    //            //                        } else {
    //            //
    //            //                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
    //            //                                    DiagnosticsEngine::Error,
    //            //                                    "Unknown attribute argument '%0'!"));
    //            //                            DB.AddString(attr_arg->getString());
    //            //                            return false;
    //            //                        }
    //            //
    //            //                        //                        std::cout << attr_arg->getString().str() << "!!!!!!\n";
    //            //                        //                        ns->dumpColor();
    //            //                    }
    //            //                }
    //            //                return true;
    //            //            }
    //
    //
    //            if (!memsafe.isStartDefine()) {
    //                // If the plugin does not start to be defined, we do not process or check anything else.
    //                return true;
    //            }
    //
    //
    //            if (const CXXRecordDecl * base = Result.Nodes.getNodeAs<CXXRecordDecl>("cls")) {
    //
    //                if (AnnotateAttr * attr = base->getAttr<AnnotateAttr>()) {
    //                    if (attr->getAnnotation() != "memsafe") {
    //                        return true;
    //                    }
    //
    //                    //  Mark only
    //                    checkUnsafe(*Result.Context, *base);
    //                    //                    if (checkUnsafe(*Result.Context, *base)) {
    //                    //                        return true;
    //                    //                    }
    //
    //                    std::string name = base->getQualifiedNameAsString();
    //
    //                    //                    std::cout << "base->getQualifiedNameAsString(): " << base->getNameAsString() << "\n";
    //                    //                    std::cout.flush();
    //
    //                    //bool 	isInAnonymousNamespace () const
    //                    if (name.find("(anonymous namespace)") != std::string::npos) {
    //
    //                        Diag.Report(base->getLocation(), Diag.getCustomDiagID(
    //                                DiagnosticsEngine::Error,
    //                                "Define classes in anonymous namespaces are not supported yet!"));
    //                        return false;
    //                    }
    //
    //
    //                    static std::string list;
    //                    if (list.empty()) {
    //                        for (auto &elem : memsafe.attArgs) {
    //
    //                            if (!list.empty()) {
    //                                list += "', '";
    //                            }
    //                            list += elem;
    //                        }
    //                        list.insert(0, "'");
    //                        list += "'";
    //                    }
    //
    //                    if (attr->args_size() != 1) {
    //                        Diag.Report(base->getLocation(), Diag.getCustomDiagID(
    //                                DiagnosticsEngine::Error,
    //                                "Expects one string argument from the following list: %0"))
    //                                .AddString(list);
    //                        return false;
    //                    }
    //
    //                    if (StringLiteral * str = dyn_cast_or_null<StringLiteral>(*attr->args_begin())) {
    //
    //                        auto found = memsafe.attArgs.find(str->getString().str());
    //                        if (found == memsafe.attArgs.end()) {
    //                            clang::DiagnosticBuilder DB = Diag.Report(base->getLocation(), Diag.getCustomDiagID(
    //                                    DiagnosticsEngine::Error,
    //                                    "Unknown argument '%0'. Expected string argument from the following list: %1"));
    //                            DB.AddString(str->getString().str());
    //                            DB.AddString(list);
    //                        }
    //
    //
    //                        appendDecl(*base, str->getString().str());
    //
    //                    }
    //                    //                    base->dumpColor();
    //
    //                    //                msBaseList.insert(name);
    //                    //                    std::cout << "Class type : " << name << "   " << attr->args_size() << " ";
    //                    //                    for (auto &elem : attr->args()) {
    //                    //                        //                        StringLiteral *Literal = dyn_cast_or_null<StringLiteral>(elem);
    //                    //
    //                    //                        if (StringLiteral * Literal = dyn_cast_or_null<StringLiteral>(elem)) {
    //                    //                            std::cout << Literal->getString().str();
    //                    //                        } else if (auto opt = elem->getIntegerConstantExpr(*Result.Context)) {
    //                    //                            if (opt) {
    //                    //                                std::cout << opt->getExtValue() << " Integer\n";
    //                    //                            }
    //                    //                            //                            std::cout << Integer->getLocation().printToString(*Result.SourceManager); //getSpelling();//getExprStmt()->getValue();
    //                    //                        } else {
    //                    //                            std::cout << " UNKNOWN '";
    //                    //                            //                            std::cout << elem->getValueKind ();
    //                    //                            //                            std::cout << elem->getLocation().printToString(*Result.SourceManager); //getSpelling();//getExprStmt()->getValue();
    //                    //                            std::cout << "'";
    //                    //                        }
    //                    //
    //                    //                        //                        std::cout << elem->getName();
    //                    //                        std::cout << " ";
    //                    //                    }
    //                    //                    std::cout << "\n";
    //
    //
    //                }
    //                return true;
    //
    //            } else if (const FunctionDecl * func = Result.Nodes.getNodeAs<FunctionDecl>("func")) {
    //
    //                if (checkAnnotate(func, "memsafe")) {
    //
    //                    Diag.Report(base->getLocation(), Diag.getCustomDiagID(
    //                            DiagnosticsEngine::Error,
    //                            "Using regular functions for the memory safety plugin instead of template classes is not supported yet!"));
    //                    return false;
    //                }
    //
    //                //                    // Конструкторы и методы класса memsafe::Variable
    //                //
    //                //                    std::cout << " FunctionDecl FunctionDecl !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    //                //                    func->dumpColor();
    //                return true;
    //            }
    //
    //
    //            if (!memsafe.isEnabled()) {
    //                // If the plugin is disabled, we don't process or check anything anymore.
    //                return true;
    //            }
    //
    //
    //
    //
    //
    //            if (const CXXOperatorCallExpr * assign = Result.Nodes.getNodeAs<CXXOperatorCallExpr>("assign")) {
    //                return checkAssignOp(*assign);
    //            }
    //
    //
    //
    //
    //            if (const FieldDecl * field = Result.Nodes.getNodeAs<FieldDecl>("field")) {
    //
    //                if (checkAnnotate(field, "memsafe")) {
    //
    //                    // Анализ для полей классов и структур пока не реализован
    //
    //
    //                    field->dumpColor();
    //
    //                    DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
    //                    Diag.Report(field->getLocation(), Diag.getCustomDiagID(
    //                            DiagnosticsEngine::Error, "Field not implemented!"));
    //
    //                    if (checkUnsafe(*Result.Context, *field)) {
    //                        return true;
    //                    }
    //                }
    //
    //            } else if (const VarDecl * create = Result.Nodes.getNodeAs<VarDecl>("create")) {
    //
    //                //                if (create->getName().find("memsafe::Variable") == std::string::npos) {
    //                //                    return;
    //                //                }
    //                if (checkUnsafe(*Result.Context, *create)) {
    //                    // Mark and ignore
    //                    return true;
    //                }
    //
    //                FIXIT_DIAG(create->getLocation(), "Create variable", "create");
    //                //                Diag.Report(create->getLocation(), Diag.getCustomDiagID(clang::DiagnosticsEngine::Remark, "Please rename this")).
    //                //                        AddFixItHint(clang::FixItHint::CreateInsertion(create->getLocation(), "/* processed */ "));
    //
    //                std::cout << "VarDecl CXXConstructExpr !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! " << std::endl;
    //                create->dumpColor();
    //
    //

    //            return true;
    //        }
    //
    //        /*
    //         * 
    //         * 
    //         */

    //
    //
    //    };

    /*
     * 
     * 
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

        // A list of attribute parameters that the memory safety model will work with, 
        // as well as the names of the class templates that are used to implement each data type.
        //     
        // Template class names are missing until they are defined during plugin initialization 
        // in a namespace with the memsafe attribute and the 'define' argument 
        // (i.e. namespace [[memsafa("define")]] ... )

        static inline const char * VALUE = "value";
        static inline const char * SHARE = "share";
        static inline const char * WEAK = "weak";
        static inline const char * AUTO = "auto";

        static inline const std::set<std::string> attArgs{VALUE, SHARE, WEAK, AUTO};


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
     * Проблема типового подхода к анализу AST с помощью visitor и matcher в следующем.
     * Оба способа ориентированы на поиск отдельных узлов по заранее определнным условиям,
     * из-за чего применение некоторый условий поиска затруднительно из-за их взаимой связи.
     * 
     * Можно было бы обойтись реализацией единственного match anyOf(decl(),stmt()), но это невозможно 
     * из-за разного типа возвращаемого значения  (Input value has unresolved overloaded type: Matcher<Decl>&Matcher<Stmt>).
     * 
     * Решением является реализация собственного RecursiveASTVisitor,
     * в котором проход применяет условия поиска к каждому узлу последовательно,
     * с учетом текущего состояния объекта и не зависит от типа анализируемого узла
     * (и это понятнее, чем вручную получать Stmt с помощью getBody у Decl для Matchers).
     * 
     */

    class MemSafePlugin : public RecursiveASTVisitor<MemSafePlugin>, protected MemSafe {
    protected:
        CompilerInstance &Instance;

    public:

        MemSafePlugin(CompilerInstance &I) : Instance(I) {
        }

#define FIXIT_DIAG(location, remark, type, ... ) \
            Instance.getDiagnostics().Report(location, Instance.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Remark, remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type __VA_OPT__(,) __VA_ARGS__)))

#define FIXIT_ERROR(location, remark, type, ... ) \
            Instance.getDiagnostics().Report(location, Instance.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, remark)).\
                AddFixItHint(clang::FixItHint::CreateInsertion(location, MakeFixitMessage(type __VA_OPT__(,) __VA_ARGS__)))

        std::string MakeFixitMessage(const std::string_view type, int level = VarInfo::NOLEVEL, const VarInfo::DeclType parent = std::monostate()) {
            std::string message("/* [[memsafe(\"");
            message += type.begin();
            message += "\"";
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

                    StringLiteral *attr_arg = nullptr;
                    if (attr->args_size() == 1) {
                        attr_arg = dyn_cast_or_null<StringLiteral>(*attr-> args_begin()); //getArgAsExpr(0));
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

            if(value){
                type = value->getType ();
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
                        && std::string_view(MemSafe::SHARE).compare(found_left) == 0
                        && std::string_view(MemSafe::SHARE).compare(found_right) == 0) {

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

                //            const Expr *expr_left = dyn_cast_or_null<Expr>(assign->getArg(0));
                //            const Expr *expr_right = dyn_cast_or_null<Expr>(assign->getArg(1));
                //
                const DeclRefExpr * left;
                MemSafe::VAR_LIST_ITER var_left;
                const char * found_left = expandAssignArgs(assign.getArg(0), left, var_left);

                const DeclRefExpr * right;
                MemSafe::VAR_LIST_ITER var_right;
                const char * found_right = expandAssignArgs(assign.getArg(1), right, var_right);


                if (std::string_view(MemSafe::SHARE).compare(found_left) == 0
                        && std::string_view(MemSafe::SHARE).compare(found_right) == 0) {

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
                //                    if (checkUnsafe(*Result.Context, *base)) {
                //                        return true;
                //                    }

                std::string name = base.getQualifiedNameAsString();

                //                    std::cout << "base->getQualifiedNameAsString(): " << base->getNameAsString() << "\n";
                //                    std::cout.flush();

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

            // Определение обычных переменных
            // Тут нужно маркировать глубину вложенности каждой из них для поседущего анализа в дельнейшем

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

//            llvm::outs() << "\nclang::DynTypedNodeList NodeList = Result.Context->getParents(*var)\n";

            if (var.getStorageDuration() != SD_Static) {
                parent_name = findParenName(Instance.getASTContext().getParents(var), level);
            }

            AddVariable(var, found_type, level, parent_name);
            FIXIT_DIAG(var.getLocation(), "Create variable", found_type, level, parent_name);

            return true;
        }

        bool checkReturnStmt(const ReturnStmt & ret) {


            //            matcher.addMatcher(callExpr(callee(functionDecl(hasName("swap")))).bind("swap"), &hMemSafe);


            if (const ExprWithCleanups * inplace = dyn_cast_or_null<ExprWithCleanups>(ret.getRetValue())) {
                FIXIT_DIAG(inplace->getBeginLoc(), "Return inplace", "approved");
                return true;
            }

            //return arg;
            //CXXConstructExpr 0x7c2e294c3808 'memsafe::VarShared<int>':'class memsafe::VarShared<int>' 'void (VarShared<int> &)'
            //`-DeclRefExpr 0x7c2e294c37e8 'memsafe::VarShared<int>':'class memsafe::VarShared<int>' lvalue ParmVar 0x7c2e294c0aa0 'arg' 'memsafe::VarShared<int>':'class memsafe::VarShared<int>'

            if (const CXXConstructExpr * expr = dyn_cast_or_null<CXXConstructExpr>(ret.getRetValue())) {


                //                FIXIT_DIAG(assign->getOperatorLoc(), "Assign value", "???");
                //
                const char * found_type = checkClassName(expr->getType().getAsString());
                //                    std::cout << "ReturnStmt CXXConstructExpr " << expr->getType().getAsString() << " " << found_type << " !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n";
                //                    std::cout.flush();
                //                    expr->dumpColor();
                if (!found_type) {
                    return true;
                }

                if (std::string_view(MemSafe::AUTO).compare(found_type) == 0) {
                    FIXIT_ERROR(ret.getRetValue()->getExprLoc(), "Return auto type", "error");
                    return true;
                } else if (std::string_view(MemSafe::SHARE).compare(found_type) == 0) {
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
    private:

        MemSafePlugin myclass;

        bool DoRewrite;
        FixItRewriterOptions FixItOptions;

        /*
         * 
         * 
         * 
         * 
         */

        // const DeclContext * 	getDeclContext () const
        // const TranslationUnitDecl * 	getTranslationUnitDecl () const
        // bool 	isFunctionOrFunctionTemplate () const
        // const FunctionDecl * 	getAsFunction () const
        // TemplateDecl * 	getDescribedTemplate () const
        // virtual Stmt * 	getBody () const 

        void HandleTranslationUnit(ASTContext &context) override {

            std::unique_ptr<clang::FixItRewriter> Rewriter;

            if (DoRewrite) {

                Rewriter = std::make_unique<clang::FixItRewriter>(context.getDiagnostics(),
                        context.getSourceManager(),
                        context.getLangOpts(),
                        &FixItOptions);

                assert(Rewriter);

                context.getDiagnostics().setClient(Rewriter.get(), /*ShouldOwnClient=*/false);

            }

            myclass.TraverseDecl(context.getTranslationUnitDecl());



            // The FixItRewriter is quite a heavy object, so let's
            // not create it unless we really have to.

            if (Rewriter) {

                Rewriter->WriteFixedFiles();
            }

        }






    public:

        MemSafePluginASTConsumer(CompilerInstance & Instance) :
        myclass(Instance), DoRewrite(true), FixItOptions(".memsafe") {


            //            // https://clang.llvm.org/docs/LibASTMatchersReference.html
            //            //    Node Matchers: Matchers that match a specific type of AST node.
            //            //    Narrowing Matchers: Matchers that match attributes on AST nodes.
            //            //    Traversal Matchers: Matchers that allow traversal between AST nodes.
            //
            //            matcher.addMatcher(namespaceDecl().bind("ns"), &hMemSafe);
            //
            //
            //            // A non-instant template is not needed for analysis. And an instantiated template always has cxxRecordDecl
            //            // matcher.addMatcher(classTemplateSpecializationDecl(hasAnyBase(hasType(cxxRecordDecl(hasName(memsafeBase))))).bind("templ"), &hMemSafe);
            //            //            matcher.addMatcher(cxxRecordDecl(hasAnyBase(hasType(cxxRecordDecl(hasName(memsafeBase))))).bind("base"), &hMemSafe);
            //            matcher.addMatcher(cxxRecordDecl().bind("cls"), &hMemSafe);
            //
            //
            //            //            varDecl(
            //            //                    has(
            //            //                    cxxConstructExpr()
            //            //                    )
            //            //                    , hasType(
            //            //                    classTemplateSpecializationDecl().bind(sp_dcl_bd_name_)
            //            //                    )
            //            //                    ).bind("var");
            //            //            matcher.addMatcher(
            //            //                    varDecl(
            //            //                    has(cxxConstructExpr())
            //            //                    , hasType(
            //            //                    classTemplateSpecializationDecl()//.bind(memsafeBase)
            //            //                    )).bind("create"), &hMemSafe);
            //            //            matcher.addMatcher(cxxConstructExpr().bind("create"), &hMemSafe);
            //
            //
            //            //matcher.addMatcher(recordDecl().bind("base"), &hMemSafe);
            //
            //            matcher.addMatcher(functionDecl().bind("func"), &hMemSafe);
            //            matcher.addMatcher(varDecl().bind("var"), &hMemSafe);
            //            matcher.addMatcher(fieldDecl().bind("field"), &hMemSafe);
            //
            //
            //
            //            matcher.addMatcher(callExpr(callee(functionDecl(hasName("operator=")))).bind("assign"), &hMemSafe); //
            //            //            matcher.addMatcher(binaryOperator().bind("assign"), &hMemSafe);
            //            //            matcher.addMatcher(binaryOperator(hasOperatorName("=")).bind("op"), &hMemSafe);
            //
            //            matcher.addMatcher(returnStmt().bind("ret"), &hMemSafe);
            //            matcher.addMatcher(callExpr(callee(functionDecl(hasName("swap")))).bind("swap"), &hMemSafe);
            //
            //
            //
            //
            //
            //
            //            //            matcher.addMatcher(compoundStmt().bind("compoundStmt"), &hMemSafe);
            //            //            matcher.addMatcher(returnStmt().bind("returnStmt"), &hMemSafe);
            //
            //            //            matcher.addMatcher(objcPropertyDecl().bind("objcPropertyDecl"), &hMemSafe);
            //            //
            //            //            matcher.addMatcher(binaryOperator(hasOperatorName("=")).bind("binaryOperator"), &hMemSafe);
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

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {

            return std::unique_ptr<MemSafePluginASTConsumer>(new MemSafePluginASTConsumer(Compiler));
        }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
            std::cout << std::unitbuf;

            //            for (auto &elem : args) {
            //                if (elem.compare("base=") > 0) {
            //                    memsafeBase = elem.substr(5);
            //                    if (memsafeBase.empty()) {
            //                        std::cerr << "A base class memory safety plugin cannot have an empty name!";
            //                        return false;
            //                    }
            //                } else {
            //                    // Включение вывода диагностических сообщений о каждом найденном совпадении в AST ?????????????
            //                    std::cerr << "Unknown plugin argument: '" << elem << "'!";
            //                    return false;
            //                }
            //            }
            //
            //            std::cout << "The memory safety plugin uses the '" << memsafeBase << "' base template.\n";
            //            std::string temp(memsafeBase);
            //            temp += "<"; // For template name
            //            msBaseList.insert(temp);

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A("memsafe", "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<MemSafePluginASTAction> S("memsafe", "Memory safety plugin");


