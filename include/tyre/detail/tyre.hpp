#ifndef TYRE_DETAIL_TYRE_H
#define TYRE_DETAIL_TYRE_H

#include <any>
#include <tuple>
#include <utility>
#include <iterator>
#include <functional>
#include <type_traits>

namespace tyre
{
    template <typename VP>
    class any;
    template <typename... Vs>
    class visitor_list;

    namespace detail
    {
        // general utilities
        template <typename... Ts>
        struct type_list {};

        template <typename T>
        using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

        struct null_t {};
        constexpr inline null_t null {};
        template <typename T>
        constexpr inline bool is_null_v = std::is_same_v<remove_cvref_t<T>, null_t>;

        template <typename T>
        constexpr inline bool is_any_v = false;
        template <typename T>
        constexpr inline bool is_any_v<tyre::any<T>> = true;
        template <typename Any, typename T>
        constexpr inline bool is_compatible_v = ! is_any_v<remove_cvref_t<T>> || std::is_same_v<remove_cvref_t<Any>, remove_cvref_t<T>>;

        template <typename F>
        struct return_type;
        template <typename R, typename... Args>
        struct return_type<R(Args...)>
        {
            using type = R;
        };

        template <typename Sig, typename Func>
        struct is_invocable_sig;
        template <typename R, typename Func, typename... Args>
        struct is_invocable_sig<R(Args...), Func> : std::is_invocable_r<R, Func, Args...> {};

        // construction utilities
        template <typename Sig, typename T>
        struct transform_sig;
        template <typename R, typename T, typename... Args>
        struct transform_sig<R(Args...), T>
        {
            using return_type = std::conditional_t<std::is_same_v<R, std::any>, T, R>;
            template <typename Arg>
            using arg_type = std::conditional_t<
                std::is_same_v<std::remove_reference_t<Arg>, std::any const>, T const&,
                std::conditional_t<std::is_same_v<std::remove_reference_t<Arg>, std::any>, T&, Arg>
            >;
            using type = return_type(arg_type<Args>...);
        };

        template <typename T, typename U>
        decltype(auto) transform_arg(U&& arg)
        {
            if constexpr (std::is_same_v<remove_cvref_t<U>, std::any>) { return *std::any_cast<T>(&arg); }
            else { return std::forward<U>(arg); }
        }

        template <typename T, typename U>
        decltype(auto) transform_arg(T&& arg, U&& transformer)
        {
            if constexpr (is_any_v<remove_cvref_t<T>>) { return std::forward<U>(transformer)(std::forward<T>(arg)); }
            else { (void)transformer; return std::forward<T>(arg); }
        }

        template <typename T, typename R, typename V>
        constexpr auto make_function(V const& visitor)
        {
            return [visitor](auto&&... args) -> R
            {
                return visitor.function(transform_arg<T>(std::forward<decltype(args)>(args))...);
            };
        }
        template <typename T, typename Vs, typename... Rs, std::size_t... Is>
        constexpr auto make_functions(Vs const& visitors, type_list<Rs...>, std::index_sequence<Is...>)
        {
            return std::make_tuple(make_function<T, Rs>(std::get<Is>(visitors))...);
        }

        // visitation utilities
        template <typename Container>
        constexpr auto index(Container const& c)
        {
            using std::begin;
            using std::end;
            auto it = begin(c);
            for (; it != end(c); ++it) { if (*it) { break; } }
            return it - begin(c);
        }

        template <typename... Ts>
        decltype(auto) get_any(Ts&&... args)
        {
            constexpr bool is_any[] = { is_any_v<remove_cvref_t<Ts>>... };
            if constexpr (index(is_any) >= sizeof...(Ts)) { return null_t(); }
            else { return std::get<index(is_any)>(std::forward_as_tuple(std::forward<Ts>(args)...)); }
        }

        template <typename Tag, typename T, typename... Vs>
        decltype(auto) get_tag(T const* tuple, visitor_list<Vs...> const&)
        {
            constexpr bool is_tag[] = { std::is_same_v<Tag, typename Vs::tag_type>... };
            if constexpr (index(is_tag) >= sizeof...(Vs)) { return null_t(); }
            else { return std::get<index(is_tag)>(*tuple); }
        }

        // non-owning reference to a callable
        template <typename F>
        class function_ref;
        template <typename R, typename... Args>
        class function_ref<R(Args...)>
        {
            template <typename F>
            using enable_if_t = std::enable_if_t<! std::is_same_v<std::decay_t<F>, function_ref> && std::is_invocable_r_v<R, F&&, Args...>>;

        public:
            using result_type = R;

            constexpr function_ref() noexcept = delete;
            constexpr function_ref(function_ref const&) noexcept = default;
            constexpr function_ref& operator =(function_ref const&) noexcept = default;

            friend constexpr void swap(function_ref& a, function_ref& b) noexcept
            {
                std::swap(a.m_object, b.m_object);
                std::swap(a.m_callback, b.m_callback);
            }

            template <typename F, typename = enable_if_t<F>>
            constexpr function_ref(F&& func) noexcept :
                m_object(const_cast<void*>(static_cast<void const*>(std::addressof(func)))),
                m_callback([](void* object, Args... args) -> R
                {
                    return std::invoke(*static_cast<std::add_pointer_t<F>>(object), std::forward<Args>(args)...);
                })
            {}

            template <typename F, typename = enable_if_t<F>>
            constexpr function_ref& operator =(F&& func) noexcept
            {
                m_object = const_cast<void*>(static_cast<void const*>(std::addressof(func)));
                m_callback = [](void* object, Args... args) -> R
                {
                    return std::invoke(*static_cast<std::add_pointer_t<F>>(object), std::forward<Args>(args)...);
                };
            }

            R operator ()(Args... args) const { return m_callback(m_object, std::forward<Args>(args)...); }

        private:
            using callback_t = R (*)(void*, Args...);
            void* m_object = nullptr;
            callback_t m_callback = nullptr;
        };

        // SFINAE utilities
        template <typename T>
        constexpr bool is_in_place_type_v = false;
        template <typename T>
        constexpr bool is_in_place_type_v<std::in_place_type_t<T>> = true;
        template <typename T>
        using is_copy_constructible = std::bool_constant<! is_any_v<remove_cvref_t<T>> && ! std::is_same_v<std::decay_t<T>, std::any> && std::is_copy_constructible_v<std::decay_t<T>> && ! is_in_place_type_v<std::decay_t<T>>>;
        template <typename T>
        using is_copy_assignable = std::bool_constant<! is_any_v<remove_cvref_t<T>> && ! std::is_same_v<std::decay_t<T>, std::any> && std::is_copy_constructible_v<std::decay_t<T>>>;
        template <typename T, typename... Ts>
        using is_constructible = std::bool_constant<std::is_constructible_v<std::decay_t<T>, Ts...> && std::is_copy_constructible_v<std::decay_t<T>>>;

        // workarounds for a variadic template friend bug in clang <= 7.x
        template <typename Vs>
        struct make_any_helper
        {
            template <typename T>
            using is_invocable = typename Vs::template is_invocable<T>;
        };
        template <typename Any>
        struct visit_helper
        {
            static constexpr auto s_visitors = Any::s_visitors;
            template <typename T>
            static decltype(auto) m_vis(T&& any) { return (std::forward<T>(any).m_vis); }
            template <typename T>
            static decltype(auto) m_any(T&& any) { return (std::forward<T>(any).m_any); }
            static Any make_from(std::any&& a, typename Any::erased_functions const* v) { return Any::make_from(std::move(a), v); }
        };
    }
}

#endif
