#ifndef TYRE_TYRE_H
#define TYRE_TYRE_H

#include "tyre/detail/tyre.h"
#include <any>
#include <tuple>
#include <utility>
#include <functional>
#include <type_traits>
#include <initializer_list>

namespace tyre
{
    template <typename Tag, typename Sig, typename Func>
    struct visitor_t
    {
        using tag_type = Tag;
        using signature = Sig;
        Func function;
    };

    template <typename Tag, typename Sig, typename Func>
    constexpr auto visitor(Func function)
    {
        return visitor_t<Tag, Sig, Func> { std::move(function) };
    }

    template <typename... Vs>
    class visitor_list
    {
    public:
        constexpr visitor_list(Vs... visitors) : m_visitors(visitors...) {}

    private:
        friend detail::make_any_helper<visitor_list>;
        friend detail::make_any_helper<visitor_list const>;
        template <typename>
        friend class any;

        template <typename T>
        struct is_invocable
        {
            static constexpr bool value =
                (detail::is_invocable_sig<typename detail::transform_sig<typename Vs::signature, T>::type, decltype(Vs::function)>::value && ...);
        };

        template <typename T>
        constexpr auto make_functions() const
        {
            using return_types = detail::type_list<typename detail::transform_sig<typename Vs::signature, T>::return_type...>;
            return detail::make_functions<T>(m_visitors, return_types(), std::index_sequence_for<Vs...>());
        }

        using erased_functions = std::tuple<detail::function_ref<typename Vs::signature>...>;

        std::tuple<Vs...> m_visitors;
    };

    template <typename VP>
    class any
    {
        template <typename... Ts>
        using enable_if_t = std::enable_if_t<std::conjunction_v<Ts...>>;
        template <typename T>
        using is_invocable = typename decltype(VP::visitors)::template is_invocable<T>;

    public:
        any() noexcept = default;
        any(any const&) = default;
        any(any&&) noexcept = default;
        any& operator =(any const&) = default;
        any& operator =(any&&) noexcept = default;

        template <typename T, typename =
                  enable_if_t<detail::is_copy_constructible<T>, is_invocable<T>>>
        any(T&& value) :
            m_any(std::forward<T>(value)),
            m_vis(&s_erased_functions<detail::remove_cvref_t<T>>)
        {}

        template <typename T, typename... Ts, typename =
                  enable_if_t<detail::is_constructible<T, Ts...>, is_invocable<T>>>
        any(std::in_place_type_t<T>, Ts&&... args) :
            m_any(std::in_place_type<T>, std::forward<Ts>(args)...),
            m_vis(&s_erased_functions<T>)
        {}

        template <typename T, typename U, typename... Ts, typename =
                  enable_if_t<detail::is_constructible<T, std::initializer_list<U>&, Ts...>, is_invocable<T>>>
        any(std::in_place_type_t<T>, std::initializer_list<U> il, Ts&&... args) :
            m_any(std::in_place_type<T>, il, std::forward<Ts>(args)...),
            m_vis(&s_erased_functions<T>)
        {}

        template <typename T, typename =
                  enable_if_t<detail::is_copy_assignable<T>, is_invocable<T>>>
        any& operator =(T&& value)
        {
            m_any = std::forward<T>(value);
            m_vis = &s_erased_functions<detail::remove_cvref_t<T>>;
        }

        template <typename T, typename... Ts, typename =
                  enable_if_t<detail::is_constructible<T, Ts...>, is_invocable<T>>>
        void emplace(Ts&&... args)
        {
            m_any.template emplace<T>(std::forward<Ts>(args)...);
            m_vis = &s_erased_functions<T>;
        }

        template <typename T, typename U, typename... Ts, typename =
                  enable_if_t<detail::is_constructible<T, std::initializer_list<U>&, Ts...>, is_invocable<T>>>
        void emplace(std::initializer_list<U> il, Ts&&... args)
        {
            m_any.template emplace<T>(il, std::forward<Ts>(args)...);
            m_vis = &s_erased_functions<T>;
        }

        void swap(any& other) noexcept
        {
            using std::swap;
            swap(m_any, other.m_any);
            swap(m_vis, other.m_vis);
        }

        void reset() noexcept { *this = any(); }

        bool has_value() const noexcept { return m_any.has_value(); }

        std::type_info const& type() const noexcept { return m_any.type(); }

    private:
        friend detail::visit_helper<any>;
        template <typename T, typename U>
        friend decltype(auto) any_cast(U&& any);

        static constexpr auto s_visitors = VP::visitors;
        using erased_functions = typename decltype(s_visitors)::erased_functions;
        template <typename T>
        static constexpr auto s_functions = s_visitors.template make_functions<T>();
        template <typename T>
        static constexpr erased_functions s_erased_functions = s_functions<T>;

        static any make_from(std::any&& a, erased_functions const* v)
        {
            any result;
            result.m_any = std::move(a);
            result.m_vis = v;
            return result;
        }

        std::any m_any;
        erased_functions const* m_vis = nullptr;
    };

    template <typename Tag, typename... Ts>
    decltype(auto) visit(Ts&&... args)
    {
        auto&& first_any = detail::get_any(std::forward<Ts>(args)...);
        static_assert(! detail::is_null_v<decltype(first_any)>, "at least one argument to tyre::visit must be a tyre::any");
        static_assert((detail::is_compatible_v<decltype(first_any), Ts> && ...), "all tyre::any arguments must have the same type");

        using helper = detail::visit_helper<detail::remove_cvref_t<decltype(first_any)>>;
        if (! helper::m_vis(first_any)) { throw std::bad_any_cast(); }
        auto&& func = detail::get_tag<Tag>(helper::m_vis(first_any), helper::s_visitors);
        static_assert(! detail::is_null_v<decltype(func)>, "this tyre::any does not support that visitor");

        constexpr auto transformer = [](auto&& any) -> decltype(auto) { return helper::m_any(any); };
        static_assert(std::is_invocable_v<decltype((func)), decltype(detail::transform_arg(std::forward<Ts>(args), transformer))...>,
            "no match to the visitor's parameter list for these argument types");
        if constexpr (std::is_same_v<std::any, typename detail::remove_cvref_t<decltype(func)>::result_type>)
        {
            std::any any = std::invoke(func, detail::transform_arg(std::forward<Ts>(args), transformer)...);
            if (&any.type() != &first_any.type()) { throw std::bad_any_cast(); }
            return helper::make_from(std::move(any), helper::m_vis(first_any));
        }
        else { return std::invoke(func, detail::transform_arg(std::forward<Ts>(args), transformer)...); }
    }

    template <typename VP>
    void swap(any<VP>& a, any<VP>& b) noexcept
    {
        a.swap(b);
    }

    template <typename T, typename U>
    decltype(auto) any_cast(U&& any)
    {
        return std::any_cast<T>(std::forward<U>(any).m_any);
    }

    template <typename VP, typename T, typename... Ts>
    any<VP> make_any(Ts&&... args)
    {
        static_assert(detail::make_any_helper<decltype(VP::visitors)>::template is_invocable<T>::value,
            "visitor function does not match its signature after substituting T for std::any");
        static_assert(std::is_copy_constructible_v<T>, "T is not copy constructible");
        static_assert(detail::is_constructible<T, Ts...>::value,
            "no matching constructor of T for these arguments");
        return any<VP>(std::in_place_type<T>, std::forward<Ts>(args)...);
    }

    template <typename VP, typename T, typename U, typename... Ts>
    any<VP> make_any(std::initializer_list<U> il, Ts&&... args)
    {
        static_assert(detail::make_any_helper<decltype(VP::visitors)>::template is_invocable<T>::value,
            "visitor function does not match its signature after substituting T for std::any");
        static_assert(std::is_copy_constructible_v<T>, "T is not copy constructible");
        static_assert(detail::is_constructible<T, std::initializer_list<U>&, Ts...>::value,
            "no matching constructor of T for these arguments");
        return any<VP>(std::in_place_type<T>, il, std::forward<Ts>(args)...);
    }
}

#endif
