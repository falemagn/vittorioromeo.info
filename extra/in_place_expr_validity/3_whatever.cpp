#include <type_traits>
#include <iostream>
#include <tuple>
#include <utility>
#include <experimental/tuple>
#include <boost/hana.hpp>
#include <vrm/pp.hpp>

struct A
{
    A() = delete;
    void m0();
};

struct B
{
    void m1();
};

int operator+(A, B);

template <typename T>
struct type_w
{
    using type = T;
};

template <typename T>
constexpr type_w<T> type_c{};

template <typename...>
using int_t = int;

template <typename TF>
struct validimpl
{
    template <typename... Ts>
    constexpr auto operator()(Ts... ts)
    {
        return decltype(std::is_callable<std::decay_t<TF>(
                typename decltype(ts)::type...)>{}){};
    }
};

template <typename T, typename TF>
constexpr auto operator|(T x, validimpl<TF> v)
{
    return std::apply(v, x);
}

template <typename TF>
constexpr auto is_valid(TF)
{
    return validimpl<TF>{};
}


template <typename... Ts>
constexpr auto type_deferrer(Ts... xs)
{
    return std::make_tuple(xs...);
}


#define WAT(...) is_valid([](auto _0) constexpr->decltype(__VA_ARGS__){})
#define IVTEST(type0) type_deferrer(type_c<type0>) | WAT

#define WAT2(...) is_valid([](auto _0, auto _1) constexpr->decltype(__VA_ARGS__){})
#define IVTEST2(type0, type1) type_deferrer(type_c<type0>, type_c<type1>) | WAT2



/*


template <typename TF, typename T>
constexpr bool is_valid2(TF, T)
{
    if constexpr(can_invoke<std::decay_t<TF>(int_t<typename T::type>)>{})
    {
        return std::true_type{};
    }
    else
    {
        return std::false_type{};
    }
}   */





#define VR_IS_VALID_1(type0, expression) \
    is_valid([](auto _0) constexpr->decltype expression{}, type_c<type0>)

#define IS_VALID_0(type0, ...) \
    is_valid([](auto _0) constexpr->decltype(__VA_ARGS__){})(type_c<type0>)

#define IS_VALID_1(type0, type1, ...)                                  \
    is_valid([](auto _0, auto _1) constexpr->decltype(__VA_ARGS__){})( \
        type_c<type0>, type_c<type1>)
/*
template <typename T0, typename T1>
void sum_if_possible(T0 a, T1 b)
{
    (void) a; (void) b;

    static_assert(is_valid([](auto _0, auto _1) -> decltype(_0 + _1)
        {
        })(type_c<T0>, type_c<T1>));

    if
        constexpr(true)
        {
        }


    if constexpr(IS_VALID_1(T0, T1, _0 + _1))
    {
    }
    else
    {
    }

}

int main()
{
    // static_assert(is_valid([](auto _x) constexpr -> decltype(_x.m0()) { },
    // type_c<A>) == true);
    // static_assert(VR_IS_VALID_1(A, (_0.m0())) == true);
    static_assert(boost::hana::is_valid([](auto _x) -> decltype(_x.m0())
        {
        })(A{}));

    static_assert(IS_VALID_0(A, _0.m0()));
    static_assert(IS_VALID_1(A, B, _0 + _1));

    sum_if_possible(1, 2);

    // static_assert(is_valid2([](auto x) constexpr -> decltype(x) { },
    // type_c<decltype(std::declval<A>().m20())>) == true);
}*/


struct Cat
{
    Cat() = delete;
    void meow() const
    {
        std::cout << "meow\n";
    }
};

struct Dog
{
    Dog() = delete;
    void bark() const
    {
        std::cout << "bark\n";
    }
};

template <typename T>
auto make_noise(const T& x)
{
    static_assert(IVTEST(T)(_0.meow()));
    static_assert(IVTEST2(T, int)(std::make_tuple(_0, _1)));
    /*if
        constexpr(IVTEST(T)(_0.meow()))
        {
            x.meow();
        } 
      */  /* else if constexpr(IS_VALID_0(T, _0.bark()))
         {
             x.bark();
         }
         else
         {
             struct cannot_meow_or_bark;
             cannot_meow_or_bark{};
         }*/
}

int main()
{
    make_noise(Cat{});
    //make_noise(Dog{});
    // make_noise(int{});
}   