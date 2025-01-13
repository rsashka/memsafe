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

    struct VarInfo {
        static const int NOLEVEL = std::numeric_limits<int>::lowest();
        typedef std::variant<std::monostate, const FunctionDecl *, const FieldDecl *> DeclType;

        const SourceLocation &location;
        const char * const &type;
        const int level;
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
        std::map<std::string, VarInfo> varList;
        std::set<const NamespaceDecl *> unsafeSet;

        MemSafe() : m_enabled(false), m_start_define(false) {

        }

        void AddVariable(const std::string && name, const SourceLocation &location, const char * const &type, const int level, VarInfo::DeclType parent = std::monostate()) {
            if (varList.find(name) != varList.end()) {
                assert(false);
            }
            varList.emplace(name, VarInfo{location, type, level, parent});
        }

        void AddUnsafe(const NamespaceDecl *ns) {
            assert(ns);
            unsafeSet.insert(ns);
        }

        inline const char * getType(std::string_view type) {
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

        bool setEnabled(bool mode) {
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
                std::cerr << "Error: Template class for attribute [[memsafe(\"" << attr << "\")]] not defined!\n";

label_done:
                ;
            }
            m_enabled = done;
            return done;
        }

    };

    static MemSafe memsafe;

    /*
     * 
     * 
     * 
     */

    struct MemSafeAttrInfo : public ParsedAttrInfo {

        MemSafeAttrInfo() {
            OptArgs = 8;
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

            } else if (!isa<FunctionDecl>(D)) {

                S.Diag(Attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
                        << Attr << Attr.isRegularKeywordAttribute() << "functions or method";

                return false;
            }

            return true;
        }

        AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &Attr) const override {

            if (Attr.getNumArgs() > 2) {

                unsigned ID = S.getDiagnostics().getCustomDiagID(
                        DiagnosticsEngine::Error,
                        "'memsafe' attribute accept only one argument");
                S.Diag(Attr.getLoc(), ID);
                return AttributeNotApplied;
            }

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

                SmallVector<Expr *, 8> ArgsBuf;
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

            return AttributeApplied;
        }

        /*
         * @todo clang-20
         * To apply custom attributes to expressions, you need to update clang, 
         * as the current release version (19) 
         * does not have this functionality (handleStmtAttribute)
         * 
         */

    };

    /*
     * 
     * 
     * 
     */

    class MemSafeHandler : public MatchFinder::MatchCallback {
    private:
        CompilerInstance &Instance;
        DiagnosticsEngine &Diag;
        ASTContext *context;

    public:

        MemSafeHandler(CompilerInstance &Instance, DiagnosticsEngine &d) : Instance(Instance), Diag(d) {

        }

        void setContext(ASTContext &context) {
            this->context = &context;
        }

#define FIXIT_DIAG(location, remark, type, ... ) \
            Diag.Report(location, Diag.getCustomDiagID(clang::DiagnosticsEngine::Remark, remark)).\
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

        static bool checkAnnotate(const Decl * decl, std::string_view name) {
            if (!decl) {
                return false;
            }

            if (AnnotateAttr * attr = decl->getAttr<AnnotateAttr>()) {
                return attr->getAnnotation() == name.begin();
            }
            return false;
        }

        bool checkUnsafe(ASTContext &ctx, const Decl & decl) {

            clang::DynTypedNodeList parents = ctx.getParents(decl);

            while (!parents.empty()) {
                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                }

                if (const NamespaceDecl * ns = parents[0].get<NamespaceDecl>()) {
                    if (memsafe.unsafeSet.find(ns) != memsafe.unsafeSet.end()) {
                        FIXIT_DIAG(decl.getBeginLoc(), "Add mark \"unsafe\" namesapace", "unsafe");
                        return true;
                    }
                }

                parents = ctx.getParents(parents[0]);
            }
            return false;
        }

        VarInfo::DeclType findParenName(ASTContext &ctx, clang::DynTypedNodeList parents, int & level) {

            level++;

            while (!parents.empty()) {

                if (parents[0].get<TranslationUnitDecl>()) {
                    break;
                }

                if (const FunctionDecl * func = parents[0].get<FunctionDecl>()) {
                    // FunctionDecl - определение внутри функции
                    return func;
                }

                if (parents[0].get<CompoundStmt>()) {
                    // CompoundStmt - определение внутри блока кода
                    level++;
                    goto next_parent;
                }

next_parent:
                ;
                parents = ctx.getParents(parents[0]);
            }
            return std::monostate();
        }

        virtual void run(const MatchFinder::MatchResult &Result) {

            if (runAnalysis(Diag, Result)) {

            }
            
        }

        bool runAnalysis(DiagnosticsEngine &Diag, const MatchFinder::MatchResult &Result) {

            if (const NamespaceDecl * ns = Result.Nodes.getNodeAs<NamespaceDecl>("ns")) {

                // Check namespace annotation attribute
                // This check should be done first as it is used to enable and disable the plugin.

                if (AnnotateAttr * attr = ns->getAttr<AnnotateAttr>()) {
                    if (attr->getAnnotation() == "memsafe") {

                        StringLiteral *attr_arg = nullptr;
                        if (attr->args_size() == 1) {
                            attr_arg = dyn_cast_or_null<StringLiteral>(*attr-> args_begin()); //getArgAsExpr(0));
                        }
                        if (!attr_arg) {
                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "The namespace attribute must contain one string argument!"));
                            return false;
                        }


                        if (MemSafe::defineArg.compare(attr_arg->getString()) == 0) {

                            if (memsafe.isEnabled()) {
                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                        DiagnosticsEngine::Error,
                                        "You cannot define plugin classes while it is running!"));
                                return false;
                            }


                            if (memsafe.isStartDefine()) {
                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                        DiagnosticsEngine::Error,
                                        "Memory safety plugin detection is already running!"));
                                return false;
                            }

                            memsafe.startDefine();
                            return true;

                        } else if (MemSafe::enableArg.compare(attr_arg->getString()) == 0) {

                            if (memsafe.setEnabled(true)) {
                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                        DiagnosticsEngine::Remark,
                                        "Memory safety plugin is enabled!"));
                            } else {
                                clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                        DiagnosticsEngine::Error,
                                        "Error enabling memory safety plugin!"));
                                return false;

                            }
                            return true;

                        } else if (MemSafe::unsafeArg.compare(attr_arg->getString()) == 0) {

                            memsafe.AddUnsafe(ns);
                            return true;

                        } else if (MemSafe::disableArg.compare(attr_arg->getString()) == 0) {

                            memsafe.setEnabled(false);
                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                    DiagnosticsEngine::Remark,
                                    "Memory safety plugin is disabled!"));
                            return true;

                        } else {

                            clang::DiagnosticBuilder DB = Diag.Report(ns->getLocation(), Diag.getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "Unknown attribute argument '%0'!"));
                            DB.AddString(attr_arg->getString());
                            return false;
                        }

                    }
                }
                return true;
            }


            if (!memsafe.isStartDefine()) {
                // If the plugin does not start to be defined, we do not process or check anything else.
                return true;
            }


            if (const CXXRecordDecl * base = Result.Nodes.getNodeAs<CXXRecordDecl>("cls")) {

                if (AnnotateAttr * attr = base->getAttr<AnnotateAttr>()) {
                    if (attr->getAnnotation() != "memsafe") {
                        return true;
                    }

                    //  Mark only
                    checkUnsafe(*Result.Context, *base);
                    //                    if (checkUnsafe(*Result.Context, *base)) {
                    //                        return true;
                    //                    }

                    std::string name = base->getQualifiedNameAsString();

                    if (name.find("(anonymous namespace)") != std::string::npos) {

                        Diag.Report(base->getLocation(), Diag.getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "Define classes in anonymous namespaces are not supported yet!"));
                        return false;
                    }


                    static std::string list;
                    if (list.empty()) {
                        for (auto &elem : memsafe.attArgs) {

                            if (!list.empty()) {
                                list += "', '";
                            }
                            list += elem;
                        }
                        list.insert(0, "'");
                        list += "'";
                    }

                    if (attr->args_size() != 1) {
                        Diag.Report(base->getLocation(), Diag.getCustomDiagID(
                                DiagnosticsEngine::Error,
                                "Expects one string argument from the following list: %0"))
                                .AddString(list);
                        return false;
                    }

                    if (StringLiteral * str = dyn_cast_or_null<StringLiteral>(*attr->args_begin())) {

                        auto found = memsafe.attArgs.find(str->getString().str());
                        if (found == memsafe.attArgs.end()) {
                            clang::DiagnosticBuilder DB = Diag.Report(base->getLocation(), Diag.getCustomDiagID(
                                    DiagnosticsEngine::Error,
                                    "Unknown argument '%0'. Expected string argument from the following list: %1"));
                            DB.AddString(str->getString().str());
                            DB.AddString(list);
                        }

                        std::cout << "Register template class '" << name << "' with '" << str->getString().str() << "' attribute!\n";
                        name.append("<"); // For complete full template name 
                        memsafe.clsUse[name] = str->getString().str();
                    }

                }
                return true;

            } else if (const FunctionDecl * func = Result.Nodes.getNodeAs<FunctionDecl>("func")) {

                if (checkAnnotate(func, "memsafe")) {

                    Diag.Report(base->getLocation(), Diag.getCustomDiagID(
                            DiagnosticsEngine::Error,
                            "Using regular functions for the memory safety plugin instead of template classes is not supported yet!"));
                    return false;
                }
                return true;
            }


            if (!memsafe.isEnabled()) {
                // If the plugin is disabled, we don't process or check anything anymore.
                return true;
            }


            if (const FieldDecl * field = Result.Nodes.getNodeAs<FieldDecl>("field")) {

                if (checkAnnotate(field, "memsafe")) {

                    // Анализ для полей классов и структур пока не реализован


                    field->dumpColor();

                    DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
                    Diag.Report(field->getLocation(), Diag.getCustomDiagID(
                            DiagnosticsEngine::Error, "Field not implemented!"));

                    if (checkUnsafe(*Result.Context, *field)) {
                        return true;
                    }
                }

            } else if (const VarDecl * create = Result.Nodes.getNodeAs<VarDecl>("create")) {

                if (checkUnsafe(*Result.Context, *create)) {
                    // Mark and ignore
                    return true;
                }

                FIXIT_DIAG(create->getLocation(), "Create variable", "create");


            } else if (const VarDecl * var = Result.Nodes.getNodeAs<VarDecl>("var")) {

                // Определение обычных переменных
                // Тут нужно маркировать глубину вложенности каждой из них для поседущего анализа в дельнейшем

                //hasAutomaticStorageDuration()
                //hasGlobalStorage()
                //hasThreadStorageDuration()

                QualType type = var->getType();

                std::string type_name = var->getType().getAsString();
                auto found = memsafe.clsUse.find(type_name.substr(0, type_name.find('<') + 1));

                const char * found_type = memsafe.getType(type_name);

                if (!found_type) {
                    return true;
                }

                // Mark only                
                checkUnsafe(*Result.Context, *var);


                VarInfo::DeclType parent_name = std::monostate();
                int level = 0;

                llvm::outs() << "\nclang::DynTypedNodeList NodeList = Result.Context->getParents(*var)\n";

                /* 0 - static veriables
                 * positive numbers - nesting level of the scope
                 * negative numbers - the level of data nesting (structures and classes taking into account all parents)
                 */

                if (var->getStorageDuration() != SD_Static) {

                    parent_name = findParenName(*Result.Context, Result.Context->getParents(*var), level);

                }


                memsafe.AddVariable(var->getName().str(), var->getLocation(), found_type, level, parent_name);
                FIXIT_DIAG(var->getLocation(), "Create variable", found_type, level, parent_name);


            } else if (const CXXOperatorCallExpr * assign = Result.Nodes.getNodeAs<CXXOperatorCallExpr>("assign")) {

                return true;

            } else if (const ReturnStmt * ret = Result.Nodes.getNodeAs<ReturnStmt>("ret")) {

                return true;
            }

            return true;
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
     */

    class MemSafePluginASTConsumer : public ASTConsumer {
    private:

        DiagnosticsEngine &Diag;
        MemSafeHandler hMemSafe;
        MatchFinder matcher;

        bool DoRewrite;
        FixItRewriterOptions FixItOptions;

        void HandleTranslationUnit(ASTContext &context) override {

            std::unique_ptr<clang::FixItRewriter> Rewriter;

            if (DoRewrite) {

                Rewriter = std::make_unique<clang::FixItRewriter>(Diag,
                        context.getSourceManager(),
                        context.getLangOpts(),
                        &FixItOptions);

                assert(Rewriter);

                Diag.setClient(Rewriter.get(), /*ShouldOwnClient=*/false);

            }

            hMemSafe.setContext(context);
            matcher.matchAST(context);
            // The FixItRewriter is quite a heavy object, so let's
            // not create it unless we really have to.

            if (DoRewrite) {

                assert(Rewriter != nullptr);
                Rewriter->WriteFixedFiles();

                //                std::cout << "WriteFixedFiles()" << "\n";
            }

        }


    public:

        MemSafePluginASTConsumer(CompilerInstance & Instance) : Diag(Instance.getDiagnostics()),
        hMemSafe(Instance, Diag), DoRewrite(true), FixItOptions(".memsafe") {


            // https://clang.llvm.org/docs/LibASTMatchersReference.html
            //    Node Matchers: Matchers that match a specific type of AST node.
            //    Narrowing Matchers: Matchers that match attributes on AST nodes.
            //    Traversal Matchers: Matchers that allow traversal between AST nodes.


            matcher.addMatcher(namespaceDecl().bind("ns"), &hMemSafe);


            // A non-instant template is not needed for analysis. And an instantiated template always has cxxRecordDecl
            // matcher.addMatcher(classTemplateSpecializationDecl(hasAnyBase(hasType(cxxRecordDecl(hasName(memsafeBase))))).bind("templ"), &hMemSafe);
            // matcher.addMatcher(cxxRecordDecl(hasAnyBase(hasType(cxxRecordDecl(hasName(memsafeBase))))).bind("base"), &hMemSafe);
            matcher.addMatcher(cxxRecordDecl().bind("cls"), &hMemSafe);


            matcher.addMatcher(functionDecl().bind("func"), &hMemSafe);
            matcher.addMatcher(varDecl().bind("var"), &hMemSafe);
            matcher.addMatcher(fieldDecl().bind("field"), &hMemSafe);
            matcher.addMatcher(cxxOperatorCallExpr().bind("assign"), &hMemSafe); //
            //            matcher.addMatcher(binaryOperator().bind("assign"), &hMemSafe);
            //            matcher.addMatcher(binaryOperator(hasOperatorName("=")).bind("op"), &hMemSafe);

            matcher.addMatcher(returnStmt().bind("ret"), &hMemSafe);


            //            matcher.addMatcher(compoundStmt().bind("compoundStmt"), &hMemSafe);
            //            matcher.addMatcher(returnStmt().bind("returnStmt"), &hMemSafe);

            //            matcher.addMatcher(objcPropertyDecl().bind("objcPropertyDecl"), &hMemSafe);
            //
            //            matcher.addMatcher(binaryOperator(hasOperatorName("=")).bind("binaryOperator"), &hMemSafe);
        }

    };

    class MemSafePluginASTAction : public PluginASTAction {
    public:

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {
            return std::unique_ptr<MemSafePluginASTConsumer>(new MemSafePluginASTConsumer(Compiler));
        }

        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {

            return true;
        }
    };
}

static ParsedAttrInfoRegistry::Add<MemSafeAttrInfo> A("memsafe", "Memory safety plugin control attribute");
static clang::FrontendPluginRegistry::Add<MemSafePluginASTAction> S("memsafe", "Memory safety plugin");


