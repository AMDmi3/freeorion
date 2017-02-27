#include "ValueRefParser.h"

#include "ParseImpl.h"
#include "ConditionParserImpl.h"
#include "EnumParser.h"
#include "../universe/ValueRef.h"

#include <boost/spirit/include/phoenix.hpp>


#define DEBUG_VALUEREF_PARSERS 0

// These are just here to satisfy the requirements of boost::spirit::qi::debug(<rule>).
#if DEBUG_VALUEREF_PARSERS
namespace std {
    inline ostream& operator<<(ostream& os, const std::vector<const char*>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::vector<boost::variant<ValueRef::OpType, ValueRef::ValueRefBase<int>*> >&) { return os; }
    inline ostream& operator<<(ostream& os, const std::vector<boost::variant<ValueRef::OpType, ValueRef::ValueRefBase<double>*> >&) { return os; }
}
#endif

typedef parse::detail::rule<
    ValueRef::ReferenceType ()
> reference_token_rule;

typedef parse::detail::rule<
    const char* ()
> name_token_rule;

template <typename T>
using variable_rule = parse::detail::rule<
    ValueRef::Variable<T>* (),
    boost::spirit::qi::locals<
        std::vector<std::string>,
        ValueRef::ReferenceType
    >
>;

template <typename T>
using statistic_rule = parse::detail::rule<
    ValueRef::Statistic<T>* (),
    boost::spirit::qi::locals<
        ValueRef::ValueRefBase<T>*,
        ValueRef::StatisticType
    >
>;

template <typename T>
using complex_variable_rule = parse::detail::rule<
    ValueRef::ComplexVariable<T>* (),
    boost::spirit::qi::locals<
        std::string,
        ValueRef::ValueRefBase<int>*,
        ValueRef::ValueRefBase<int>*,
        ValueRef::ValueRefBase<std::string>*,
        ValueRef::ValueRefBase<std::string>*,
        ValueRef::ValueRefBase<int>*
    >
>;

template <typename T>
using expression_rule = parse::detail::rule<
    ValueRef::ValueRefBase<T>* (),
    boost::spirit::qi::locals<
        ValueRef::ValueRefBase<T>*,
        ValueRef::ValueRefBase<T>*,
        ValueRef::OpType,
        std::vector<ValueRef::ValueRefBase<T>*>
    >
>;


const reference_token_rule&                     variable_scope();
const name_token_rule&                          container_type();

const variable_rule<int>&                       int_bound_variable();
const variable_rule<int>&                       int_free_variable();
const statistic_rule<int>&                      int_var_statistic();
const complex_variable_rule<int>&               int_var_complex();
const parse::value_ref_rule<int>&               int_simple();

const complex_variable_rule<double>&            double_var_complex();
const parse::value_ref_rule<double>&            double_simple();

const complex_variable_rule<std::string>&       string_var_complex();


template <typename T>
void initialize_nonnumeric_statistic_parser(
    statistic_rule<T>& statistic,
    const typename parse::value_ref_rule<T>& value_ref)
{
    using boost::phoenix::construct;
    using boost::phoenix::new_;
    using boost::phoenix::push_back;

    boost::spirit::qi::_1_type _1;
    boost::spirit::qi::_a_type _a;
    boost::spirit::qi::_b_type _b;
    boost::spirit::qi::_val_type _val;

    const parse::lexer& tok = parse::lexer::instance();

    statistic
        =   tok.Statistic_
            >>  tok.Mode_ [ _b = ValueRef::MODE ]
            >   parse::detail::label(Value_token)     >     value_ref [ _a = _1 ]
            >   parse::detail::label(Condition_token) >     parse::detail::condition_parser
                [ _val = new_<ValueRef::Statistic<T> >(_a, _b, _1) ]
        ;
}


template <typename T>
void initialize_bound_variable_parser(
    variable_rule<T>& bound_variable,
    const name_token_rule& variable_name)
{
    using boost::phoenix::construct;
    using boost::phoenix::new_;
    using boost::phoenix::push_back;

    boost::spirit::qi::_1_type _1;
    boost::spirit::qi::_a_type _a;
    boost::spirit::qi::_b_type _b;
    boost::spirit::qi::_val_type _val;

    bound_variable
        =   variable_scope() [ _b = _1 ] >> '.'
        >>-(container_type() [ push_back(_a, construct<std::string>(_1)) ] > '.')
        >>  variable_name    [ push_back(_a, construct<std::string>(_1)), _val = new_<ValueRef::Variable<T> >(_b, _a) ]
        ;
}


template <typename T>
struct enum_value_ref_rules {
    enum_value_ref_rules(const std::string& type_name) {
        using boost::phoenix::new_;
        using boost::phoenix::push_back;

        boost::spirit::qi::_1_type _1;
        boost::spirit::qi::_c_type _c;
        boost::spirit::qi::_d_type _d;
        boost::spirit::qi::_val_type _val;
        boost::spirit::qi::lit_type lit;

        const parse::lexer& tok = parse::lexer::instance();

        initialize_bound_variable_parser<T>(bound_variable_expr, variable_name);

        statistic_value_ref_expr
            =   constant_expr
            |   bound_variable_expr
            ;

        functional_expr
            =   (
                    (
                        (
                            tok.OneOf_      [ _c = ValueRef::RANDOM_PICK ]
                        |   tok.Min_        [ _c = ValueRef::MINIMUM ]
                        |   tok.Max_        [ _c = ValueRef::MAXIMUM ]
                        )
                        >  '('  >   expr [ push_back(_d, _1) ]
                        >*(','  >   expr [ push_back(_d, _1) ] )
                        [ _val = new_<ValueRef::Operation<T> >(_c, _d) ] >   ')'
                    )
                |   (
                        primary_expr [ _val = _1 ]
                    )
                )
            ;

        expr
            =   functional_expr
            ;

        initialize_nonnumeric_statistic_parser<T>(statistic_expr, statistic_value_ref_expr);

        primary_expr
            =   constant_expr
            |   bound_variable_expr
            |   statistic_expr
            ;

#if DEBUG_VALUEREF_PARSERS
        debug(variable_name);
        debug(constant_expr);
        debug(bound_variable_expr);
        debug(statistic_value_ref_expr);
        debug(statistic_expr);
        debug(functional_expr);
        debug(primary_expr);
        debug(expr);
#endif

        variable_name.name(type_name + " variable name");
        constant_expr.name(type_name);
        bound_variable_expr.name(type_name + " variable");
        statistic_value_ref_expr.name(type_name + " statistic value reference");
        statistic_expr.name(type_name + " statistic");
        primary_expr.name(type_name + " expression");
        expr.name(type_name + " expression");
    }

    name_token_rule variable_name;
    parse::value_ref_rule<T> constant_expr;
    variable_rule<T> bound_variable_expr;
    expression_rule<T> functional_expr;
    parse::value_ref_rule<T> primary_expr;
    parse::value_ref_rule<T> statistic_value_ref_expr;
    statistic_rule<T> statistic_expr;
    parse::value_ref_rule<T> expr;
};


template <typename T>
struct arithmetic_rules
{
    arithmetic_rules(const std::string& type_name) {
        using boost::phoenix::construct;
        using boost::phoenix::new_;
        using boost::phoenix::push_back;

        boost::spirit::qi::_1_type _1;
        boost::spirit::qi::_a_type _a;
        boost::spirit::qi::_b_type _b;
        boost::spirit::qi::_c_type _c;
        boost::spirit::qi::_d_type _d;
        boost::spirit::qi::_val_type _val;
        boost::spirit::qi::lit_type lit;

        const parse::lexer& tok = parse::lexer::instance();

        functional_expr
            =   (
                    (
                        (
                            tok.Sin_    [ _c = ValueRef::SINE ]                     // single-parameter math functions
                        |   tok.Cos_    [ _c = ValueRef::COSINE ]
                        |   tok.Log_    [ _c = ValueRef::LOGARITHM ]
                        |   tok.Abs_    [ _c = ValueRef::ABS ]
                        )
                        >> '(' >> expr [ _val = new_<ValueRef::Operation<T> >(_c, _1) ] >> ')'
                    )
                |   (
                            tok.RandomNumber_   [ _c = ValueRef::RANDOM_UNIFORM ]   // random number requires a min and max value
                        >  '(' >    expr [ _a = _1 ]
                        >  ',' >    expr [ _val = new_<ValueRef::Operation<T> >(_c, _a, _1) ] >> ')'
                    )
                |   (
                        (
                            tok.OneOf_  [ _c = ValueRef::RANDOM_PICK ]              // oneof, min, or max can take any number or operands
                        |   tok.Min_    [ _c = ValueRef::MINIMUM ]
                        |   tok.Max_    [ _c = ValueRef::MAXIMUM ]
                        )
                        >>  '(' >>  expr [ push_back(_d, _1) ]
                        >>(*(',' >  expr [ push_back(_d, _1) ] ))
                        [ _val = new_<ValueRef::Operation<T> >(_c, _d) ] >> ')'
                    )
                |   (
                        lit('-') >> functional_expr                                   // single parameter math function with a function expression rather than any arbitrary expression as parameter, because negating more general expressions can be ambiguous
                        [ _val = new_<ValueRef::Operation<T> >(ValueRef::NEGATE, _1) ]
                    )
                |   (
                        primary_expr [ _val = _1 ]
                    )
                )
            ;

        exponential_expr
            =   (
                    functional_expr [ _a = _1 ]
                    >> '^' >> functional_expr
                    [ _val = new_<ValueRef::Operation<T> >( ValueRef::EXPONENTIATE, _a, _1 ) ]
                )
            |   functional_expr [ _val = _1 ]
            ;

        multiplicative_expr
            =   (
                    exponential_expr [ _a = _1 ]
                >>   (
                       *(
                            (
                                (
                                    lit('*') [ _c = ValueRef::TIMES ]
                                |   lit('/') [ _c = ValueRef::DIVIDE ]
                                )
                            >>   exponential_expr [ _b = new_<ValueRef::Operation<T> >(_c, _a, _1) ]
                            ) [ _a = _b ]
                        )
                    )
                )
                [ _val = _a ]
            ;

        additive_expr
            =
                (
                    (
                        (
                            multiplicative_expr [ _a = _1 ]
                        >>  (
                               *(
                                    (
                                        (
                                            lit('+') [ _c = ValueRef::PLUS ]
                                        |   lit('-') [ _c = ValueRef::MINUS ]
                                        )
                                    >>   multiplicative_expr [ _b = new_<ValueRef::Operation<T> >(_c, _a, _1) ]
                                    ) [ _a = _b ]
                                )
                            )
                        )
                        [ _val = _a ]
                    )
                |   (
                        multiplicative_expr [ _val = _1 ]
                    )
                )
            ;

        statistic_collection_expr
            =   tok.Statistic_
                >> (   tok.Count_  [ _b = ValueRef::COUNT ]
                   |   tok.If_     [ _b = ValueRef::IF ]
                )
                >   parse::detail::label(Condition_token) >    parse::detail::condition_parser
                    [ _val = new_<ValueRef::Statistic<T> >(_a, _b, _1) ]
            ;

        statistic_value_expr
            =   tok.Statistic_
                >>  parse::statistic_type_enum() [ _b = _1 ]
                >   parse::detail::label(Value_token)     >     statistic_value_ref_expr [ _a = _1 ]
                >   parse::detail::label(Condition_token) >     parse::detail::condition_parser
                    [ _val = new_<ValueRef::Statistic<T> >(_a, _b, _1) ]
            ;

        statistic_expr
            =   statistic_collection_expr
            |   statistic_value_expr
            ;

        expr
            =   additive_expr
            ;

#if DEBUG_VALUEREF_PARSERS
        debug(functional_expr);
        debug(exponential_expr);
        debug(multiplicative_expr);
        debug(additive_expr);
        debug(primary_expr);
        debug(statistic_value_ref_expr);
        debug(statistic_collection_expr);
        debug(statistic_value_expr);
        debug(statistic_expr);
        debug(expr);
#endif

        functional_expr.name(type_name + " function expression");
        exponential_expr.name(type_name + " exponential expression");
        multiplicative_expr.name(type_name + " multiplication expression");
        additive_expr.name(type_name + " additive expression");
        statistic_value_ref_expr.name(type_name + " statistic value reference");
        statistic_collection_expr.name(type_name + " collection statistic");
        statistic_value_expr.name(type_name + " value statistic");
        statistic_expr.name("real number statistic");
        primary_expr.name(type_name + " expression");
        expr.name(type_name + " expression");
    }

    expression_rule<T> functional_expr;
    expression_rule<T> exponential_expr;
    expression_rule<T> multiplicative_expr;
    expression_rule<T> additive_expr;
    parse::value_ref_rule<T> primary_expr;
    parse::value_ref_rule<T> statistic_value_ref_expr;
    statistic_rule<T> statistic_collection_expr;
    statistic_rule<T> statistic_value_expr;
    statistic_rule<T> statistic_expr;
    parse::value_ref_rule<T> expr;
};
