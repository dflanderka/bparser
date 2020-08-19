/*
 * test_tools.hh
 *
 *  Created on: Jan 10, 2020
 *      Author: jb
 */

#ifndef INCLUDE_TEST_TOOLS_HH_
#define INCLUDE_TEST_TOOLS_HH_


#include "assert.hh"
#include <vector>




#define ASSERT_THROW(expression, msg) {				            \
	bool success = false;										\
    try {                                                       \
        expression;                                             \
    } catch (const bparser::Exception & e) {                    \
    	size_t pos = std::string(e.what()).find(std::string(msg)); \
		if (pos == std::string::npos) \
			std::cout << "Wrong exception msg: " << e.what() << "\n";	\
		else	\
			success = true;									    \
    } catch (const std::exception &e) {                          \
    	throw e;												\
    }                                                           \
	if ( ! success) throw;								\
}


inline bool failed_expect(bool failed) {
	static bool any_failed = false;
	any_failed = any_failed || failed;
	return any_failed;
}

#define EXPECT(EXPRESSION) \
	if ( !(EXPRESSION) ) {\
		std::cout << "Failed EXPECT, " <<  __FILE__ << ":" << __LINE__ << "\n\n"; \
		failed_expect(true); \
	}

template  <class A, class B>
bool __test_eq(A a, B b, const char * _a, const char * _b, const char * f, int l) {
	if ( !(a == b) ) {
		std::cout << "Failed EXPECT_EQ at " <<  f << ":" << l << "\n" <<
				"  " << _a <<  " = " << a << "\n  " << _b << " = " << b << "\n";
		return false;
	}
	return true;
}

#define TEST_EQ(A, B) __test_eq((A), (B), #A, #B, __FILE__, __LINE__)

inline void fill_const(double *ptr, uint n, double v) {
	for(uint i=0; i< n; ++i) ptr[i] = v;
}

inline void fill_seq(double *ptr, double a, double b, double st = 1) {
	for(uint i=0; a<b; a+=st, ++i) ptr[i] = a;
}

inline std::string print_vec(double *ptr, uint size) {
	std::stringstream s;
	s << "[";
	for(uint i=0; i<size; ++i)
		s << ptr[i] << ", " << "\n";
	s << "]";
	return s.str();
}

template< class T>
std::string print_vector(std::vector<T> x) {
	std::stringstream s;
	s << "[";
	for(uint i=0; i<x.size(); ++i)
		s << x[i] << ", " << "\n";
	s << "]";
	return s.str();
}

#endif /* INCLUDE_TEST_TOOLS_HH_ */
