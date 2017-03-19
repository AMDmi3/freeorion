#include "Parse.h"

#include "ParseImpl.h"
#include "ConditionParserImpl.h"
#include "ValueRefParser.h"

#include "../universe/Special.h"

#include <boost/spirit/include/phoenix.hpp>


#define DEBUG_PARSERS 0

#if DEBUG_PARSERS
namespace std {
    inline ostream& operator<<(ostream& os, const std::vector<std::shared_ptr<Effect::EffectsGroup>>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::map<std::string, Special*>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::pair<const std::string, Special*>&) { return os; }
}
#endif

namespace {
    struct insert_ {
        typedef void result_type;

        void operator()(std::map<std::string, Special*>& specials, Special* special) const {
            if (!specials.insert(std::make_pair(special->Name(), special)).second) {
                std::string error_str = "ERROR: More than one special in specials.txt has the name " + special->Name();
                throw std::runtime_error(error_str.c_str());
            }
        }
    };
    const boost::phoenix::function<insert_> insert;

    struct rules {
        rules() {
            namespace phoenix = boost::phoenix;
            namespace qi = boost::spirit::qi;

            using phoenix::new_;

            qi::_1_type _1;
            qi::_2_type _2;
            qi::_3_type _3;
            qi::_4_type _4;
            qi::_a_type _a;
            qi::_b_type _b;
            qi::_c_type _c;
            qi::_d_type _d;
            qi::_e_type _e;
            qi::_f_type _f;
            qi::_g_type _g;
            qi::_h_type _h;
            qi::_r1_type _r1;
            qi::_r2_type _r2;
            qi::eps_type eps;

            const parse::lexer& tok = parse::lexer::instance();

            special_prefix
                =    tok.Special_
                >    parse::detail::label(Name_token)               > tok.string [ _r1 = _1 ]
                >    parse::detail::label(Description_token)        > tok.string [ _r2 = _1 ]
                ;

            spawn
                =    (      (parse::detail::label(SpawnRate_token)   > parse::detail::double_ [ _r1 = _1 ])
                        |    eps [ _r1 = 1.0 ]
                     )
                >    (      (parse::detail::label(SpawnLimit_token)  > parse::detail::int_ [ _r2 = _1 ])
                        |    eps [ _r2 = 9999 ]
                     )
                ;

            special
                =    special_prefix(_a, _b)
                >  -(parse::detail::label(Stealth_token)            > parse::double_value_ref() [ _g = _1 ])
                >    spawn(_c, _d)
                >  -(parse::detail::label(Capacity_token)           > parse::double_value_ref() [ _h = _1 ])
                >  -(parse::detail::label(Location_token)           > parse::detail::condition_parser [ _e = _1 ])
                >  -(parse::detail::label(EffectsGroups_token)      > parse::detail::effects_group_parser() [ _f = _1 ])
                >    parse::detail::label(Graphic_token)            > tok.string
                [ insert(_r1, new_<Special>(_a, _b, _g, _f, _c, _d, _h, _e, _1)) ]
                ;

            start
                =   +special(_r1)
                ;

            special_prefix.name("Special");
            special.name("Special");
            spawn.name("SpawnRate and SpawnLimit");

#if DEBUG_PARSERS
            debug(special_prefix);
            debug(spawn);
            debug(special);
#endif

            qi::on_error<qi::fail>(start, parse::report_error(_1, _2, _3, _4));
        }

        typedef parse::detail::rule<
            void (std::string&, std::string&)
        > special_prefix_rule;

        typedef parse::detail::rule<
            void (double&, int&)
        > spawn_rule;

        typedef parse::detail::rule<
            void (std::map<std::string, Special*>&),
            boost::spirit::qi::locals<
                std::string,
                std::string,
                double,
                int,
                Condition::ConditionBase*,
                std::vector<std::shared_ptr<Effect::EffectsGroup>>,
                ValueRef::ValueRefBase<double>*,
                ValueRef::ValueRefBase<double>*
            >
        > special_rule;

        typedef parse::detail::rule<
            void (std::map<std::string, Special*>&)
        > start_rule;


        special_prefix_rule special_prefix;
        spawn_rule          spawn;
        special_rule        special;
        start_rule          start;
    };
}

namespace parse {
    bool specials(std::map<std::string, Special*>& specials_) {
        bool result = true;

        for (const boost::filesystem::path& file : ListScripts("scripting/specials")) {
            result &= detail::parse_file<rules, std::map<std::string, Special*>>(file, specials_);
        }

        return result;
    }
}
