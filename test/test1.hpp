namespace A {
typedef struct {
  int q;
} D_t;
template <class T>
class B {
 public:
  B(T v) : m(v) {}
  T m;
};
typedef B<int> B_int;
typedef B<double> B_double;

namespace E {
class C {
  int n;

 public:
  int get() { return n; }
};
}  // namespace E
}  // namespace A

typedef A::E::C AEC;
typedef A::B<int> AB_int;
typedef A::B<double> AB_double;
// full global variable definitions like this confuse c2ffi, should not be present in headers
// A::B_int moo(3);
extern A::B_int moo;
#define FOO 3
extern A::E::C bar;
const float baz = 56.3e-6f;
const double grue = 123.456;
const long double gruuu = 789.012e19L;

void topf(AB_double *abd);

typedef enum { GREEN, RED, BLUE } color_t;
typedef unsigned long int my_int;
