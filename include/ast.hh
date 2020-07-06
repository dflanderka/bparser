#ifndef INCLUDE_AST_HH_
#define INCLUDE_AST_HH_

#include <list>
#include <string>
#include <boost/variant.hpp>
#include <boost/phoenix/phoenix.hpp>
#include <boost/spirit/home/support/attributes.hpp>
#include <boost/spirit/home/qi/domain.hpp>

#include "config.hh"
#include "array.hh"

/**
 * TODO:
 * 1. We can possibly skip the AST step and directly create ScalareNode DAG through the Array interface.
 *    This needs some modifications:
 *    a) We have to find a way to have PHOENIX delayed functions replaced by delayed methdos of some object.
 *       We need one class (PrintBackend) to print AST of the grammar.
 *       We need another class (EvalBackend) to construct the evaluation DAG through the Arrays.
 *    b) Delayed construction of ValueNodes from identifiers. The `get_variables` step has to be replaced
 *       by storing uncomplete ValueNodes as a map in EvalBackend.
 *
 *    Advantage: less code, possible small speed gain in parsing phase
 *    Disadvantage: no mean for AST processing, however AST is a bit arbitrary structure anyway
 */

namespace bparser {

namespace ast {
/**
 * Abstract Syntax Tree resulting from the parsed grammar.
 * - Various helper structs and classes for AST construction (via. BOOST functional programming tools)
 * - Boost Variant type visitors for AST processing.
 * - Finally the AST is translated into DAG (direct acyclic graph) of ScalarNode operations
 *   grouped into vector structures via. Array classes.
 */


struct unary_fn {
	typedef Array (*function_type)(const Array &);
	std::string repr;
	function_type fn;
};

struct binary_fn {
	typedef Array (*function_type)(const Array &, const Array &);
	std::string repr;
	function_type fn;
};

struct ternary_fn {
	typedef Array (*function_type)(const Array &, const Array &, const Array &);
	std::string repr;
	function_type fn;
};



//struct nil {};
struct unary_op;
struct binary_op;
struct ternary_op;
struct assign_op;

// clang-format off
typedef boost::variant<
        //nil, // can't happen!  TODO: remove
        double
        , std::string
        , boost::recursive_wrapper<unary_op>
        , boost::recursive_wrapper<binary_op>
		, boost::recursive_wrapper<ternary_op>
		, boost::recursive_wrapper<assign_op>
        >
operand;
// clang-format on

/// a function: Array -> Array
struct unary_op {
    unary_fn op;
    operand first;
};

/// a function: (Array, Array) -> Array
struct binary_op {
    binary_fn op;
    operand first;
    operand second;
};

struct ternary_op {
	ternary_fn op;
	operand first;
	operand second;
	operand third;
};

struct assign_op {
    std::string lhs;
    operand rhs;
};





/**
 * operand visitors
 */


struct print_vis : public boost::static_visitor<> {
	/**
	 * Print AST.
	 */
	mutable std::stringstream ss;
	bool recursive_;

    explicit print_vis(bool recursive = true)
    : recursive_(recursive)
    {}

//    void operator()(nil UNUSED(x)) const {
//    	ss << "NULL";
//    }

    void operator()(double x) const
    {ss << x; }

    void operator()(std::string const &x) const  {
    	ss << "`" << x << "`";
    }


    void operator()(unary_op const &x) const {
    	ss << x.op.repr <<  "(";
    	if (recursive_) boost::apply_visitor(*this, x.first);
    	ss << ")";
    }

    void operator()(binary_op const &x) const {
    	ss << x.op.repr << "(";
    	if (recursive_) boost::apply_visitor(*this, x.first);
    	ss << "_";
    	if (recursive_) boost::apply_visitor(*this, x.second);
    	ss << ")";
    }

    void operator()(ternary_op const &x) const {
    	ss << x.op.repr << "(";
    	if (recursive_) boost::apply_visitor(*this, x.first);
    	ss << "_";
    	if (recursive_) boost::apply_visitor(*this, x.second);
    	ss << "_";
    	if (recursive_) boost::apply_visitor(*this, x.third);
    	ss << ")";
    }

    void operator()(assign_op const &x) const  {
    	ss << x.lhs << " = ";
    	if (recursive_) boost::apply_visitor(*this, x.rhs);
    }


};

/**
 * Print AST function
 */
inline std::string print(operand const& ast_root) {
	print_vis pv;
	boost::apply_visitor(pv, ast_root);
	return pv.ss.str();
}

inline std::ostream &operator<<(std::ostream &os, const operand& a) {
	print_vis pv(true);
	boost::apply_visitor(pv, a);
    os << pv.ss.str();
    return os;
}

/**
 * Lazy AST print.
 */
struct print_ast_t {
	typedef bool result_type;

	bool operator()(operand const& x) const {
		print(x);
		return true;
	}
};
// this and following declarations can be used with C++17
//inline boost::phoenix::function<print_ast_t> lazy_print;
BOOST_PHOENIX_ADAPT_CALLABLE(lazy_print, print_ast_t, 1)






/**
 * Construct ScalarNode operation DAG via. Arrays to expand vector operations
 * into scalar operations.
 */
struct make_array {
    typedef Array result_type;
    mutable std::map<std::string, Array> symbols;

    explicit make_array(std::map<std::string, Array> const &symbols)
    : symbols(symbols)
    {}

//    result_type operator()(nil) const {
//        BOOST_ASSERT(0);
//        return Array();
//    }

    result_type operator()(double x) const
    {
    	return Array::constant({x});
    }

    result_type operator()(std::string const &x) const  {
        auto it = symbols.find(x);
        if (it != symbols.end()) {
        	return it->second;
        } else {
        	// We do not call visitor for the assign_op's 'lhs' so this must be error.
        	Throw() << "Undefined var: " << x << "\n";
        }
    }

    result_type operator()(unary_op const &x) const {
        return x.op.fn(boost::apply_visitor(*this, x.first));
    }

    result_type operator()(binary_op const &x) const {
    	result_type first = boost::apply_visitor(*this, x.first);
    	result_type second = boost::apply_visitor(*this, x.second);
        return x.op.fn(first, second);
    }

    result_type operator()(ternary_op const &x) const {
    	result_type first = boost::apply_visitor(*this, x.first);
    	result_type second = boost::apply_visitor(*this, x.second);
    	result_type third = boost::apply_visitor(*this, x.third);
        return x.op.fn(first, second, third);
    }

    result_type operator()(assign_op x) const  {
    	//std::string& var_name = boos);
    	result_type rhs = boost::apply_visitor(*this, x.rhs);
    	symbols[x.lhs] = rhs;
        return rhs;
    }

};




/**
 * Extract free symbols/variables.
 */
struct get_variables {
    typedef std::vector<std::string> result_type;

    static result_type merge(result_type a, result_type b) {
    	result_type res(a.begin(), a.end());
    	res.insert(res.end(), b.begin(), b.end());
    	return res;
    }

    explicit get_variables()
    {}


//    result_type operator()(nil) const {
//        BOOST_ASSERT(0);
//        return {};
//    }

    result_type operator()(double UNUSED(x)) const
    { return {}; }

    result_type operator()(std::string const &x) const  {
        auto it = expr_defs.find(x);
        if (it == expr_defs.end()) {
        	return {x};
        }
        return {};
    }


    result_type operator()(unary_op const &x) const {
        return boost::apply_visitor(*this, x.first);
    }

    result_type operator()(binary_op const &x) const {
    	result_type first = boost::apply_visitor(*this, x.first);
    	result_type second = boost::apply_visitor(*this, x.second);
        return merge(first, second);
    }

    result_type operator()(ternary_op const &x) const {
        return merge(
        		merge(
        				boost::apply_visitor(*this, x.first),
						boost::apply_visitor(*this, x.second)
				),
				boost::apply_visitor(*this, x.third));

    }

    result_type operator()(assign_op const &x) const  {
    	result_type rhs = boost::apply_visitor(*this, x.rhs);
    	expr_defs.insert(x.lhs);
        return rhs;
    }



private:
    mutable std::set<std::string> expr_defs;
};





/**
 * Remove 'nil' nodes from AST. Should not be necessary.
 */
//struct remove_nil {
//    typedef operand result_type;
//
//
//    explicit remove_nil()
//    {}
//
//
//    result_type operator()(nil) const {
//        return nil();
//    }
//
//    result_type operator()(double x) const
//    { return x; }
//
//    result_type operator()(std::string const &x) const  {
//        return x;
//    }
//
//
//    result_type operator()(unary_op const &x) const {
//    	result_type first = boost::apply_visitor(*this, x.first);
//    	if (first.type() != typeid(nil)) return x;
//    	return first;
//    }
//
//    result_type operator()(binary_op const &x) const {
//    	result_type first = boost::apply_visitor(*this, x.first);
//    	if (first.type() != typeid(nil)) return x;
//    	result_type second = boost::apply_visitor(*this, x.second);
//    	if (second.type() != typeid(nil)) return x;
//    	return second;
//    }
//
//    result_type operator()(assign_op const &x) const  {
//    	BP_ASSERT(x.lhs.size() > 0);
//    	result_type rhs = boost::apply_visitor(*this, x.rhs);
//    	BP_ASSERT(rhs.type() != typeid(nil));
//        return x;
//    }
//
//
//
//};



// unary expression factory
struct make_const_f {
	// In order to minimize number of differrent operations in AST
	// the nulary 'fn' must in fact accept single double argument.
	unary_op operator()(std::string repr, ast::unary_fn::function_type fn) const {
		ast::unary_fn nulary_fn = {repr, fn};
		return {nulary_fn, 0.0}; // unused argument 0.0
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_const, make_const_f, 2)


// unary expression factory
struct make_unary_f {
	unary_op operator()(unary_fn op, operand const& first) const {
		// std::cout << "make_unary: " << first.which() << "\n";
		return {op, first};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_unary, make_unary_f, 2)


// binary expression factory
struct make_binary_f {
	binary_op operator()(binary_fn op, operand const& first, operand const& second) const {
		// std::cout << "make_binary: " << first.which() << ", " << second.which() << "\n";
		return {op, first, second};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_binary, make_binary_f, 3)

struct make_ternary_f {
	ternary_op operator()(ternary_fn op, operand const& first, operand const& second, operand const& third) const {
		// std::cout << "make_binary: " << first.which() << ", " << second.which() << "\n";
		return {op, first, second, third};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_ternary, make_ternary_f, 4)



struct make_relational_f {
	binary_op operator()(binary_fn op, operand const& first, operand const& second, operand const& chained) const {
		std::cout << "make_binary: " << first.which() << ", " << second.which() << ", " << print(chained) << "\n";
		return {op, first, second};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_relational, make_relational_f, 4)


struct make_array_constr_f {
	binary_op operator()(binary_fn op, operand const& first, operand const& second, operand const& chained) const {
		std::cout << "make_array_constr: " << first.which() << ", " << second.which() << ", " << print(chained) << "\n";
		return {op, first, second};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_array_constr, make_array_constr_f, 3)


// assign expression factory
struct make_assign_f {
	assign_op operator()(std::string lhs, operand const& rhs) const {
		return {lhs, rhs};
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(make_assign, make_assign_f, 2)

// Convert 'boost::optional' to 'operand'.
struct treat_optional_f {
	operand operator()(boost::optional<operand> const& v) const {
		if (v == boost::none) {
			return ast::make_const("None", &Array::none_array)();
		} else {
			return *v; // extract optional value
		}
	}
};
BOOST_PHOENIX_ADAPT_CALLABLE(treat_optional, treat_optional_f, 1)



inline Array semicol_fn(const Array & UNUSED(a), const Array &b) {
	return b;
}








} // namespace ast

} // namespace bparser






#endif //INCLUDE_PARSER_HH_
