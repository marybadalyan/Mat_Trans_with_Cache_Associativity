// Kaizen Version 0.1.0
// 
// MIT License
// 
// Copyright (c) 2023 Leo Heinsaar
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once 

// Since the order of these #includes doesn't matter,
// they're sorted in descending length for aesthetics
#include <unordered_map>
#include <unordered_set>
#include <forward_list>
#include <type_traits>
#include <string_view>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <optional>
#include <iostream>
#include <iterator>
#include <fstream>
#include <sstream>
#include <ostream>
#include <utility>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <regex>
#include <array>
#include <deque>
#include <ctime>
#include <queue>
#include <stack>
#include <list>
#include <set>
#include <map>

namespace zen {

///////////////////////////////////////////////////////////////////////////////////////////// MISC

// Quotes a string. This helps avoid cumbersome quote gymnastics in code.
// Example: quote(filename) + " does not exist";
// Result:  "/path/to/file" does not exist
inline std::string quote(const std::string_view s) { return '\"' + std::string(s) + '\"'; }

inline auto timestamp() {
    std::time_t result  = std::time(nullptr);
    std::string timestr = std::asctime(std::localtime(&result));
    return timestr.substr(0, timestr.length() - 1);
}

///////////////////////////////////////////////////////////////////////////////////////////// SERIALIZATION

template <class T> constexpr bool is_string_like(); // forward declaration

//------------------------------------------------------------------------------------------- std::pair

namespace internal {
    template<class T>
    std::string serialize(const T& x) {
        std::ostringstream ss;
        if constexpr (is_string_like<T>())
            ss << quote(x);
        else
            ss << x;
        return ss.str();
    }

    // Overload for std::string type: serialization for a string type means
    // simply quoting it, so that wherever it appears, it does so in quotes
    std::string serialize(const std::string& s) { return quote(s); }

    // Helper function to handle pair serialization
    template<class T1, class T2>
    std::string serialize(const std::pair<T1, T2>& p) {
        return "{" + serialize(p.first) + ", " + serialize(p.second) + "}";
    }

    // Helper function to handle pair stream output
    template<class Os, class T1, class T2>
    void pair_to_stream(Os& os, const std::pair<T1, T2>& p) {
        os << serialize(p.first) << ", " << serialize(p.second);
    }
} // namespace internal

template<class T1, class T2>
std::ostream& operator<<(std::ostream& os, const std::pair<T1, T2>& p) {
    os << "{";
    internal::pair_to_stream(os, p);
    os << "}";
    return os;
}

//------------------------------------------------------------------------------------------- std::tuple

namespace internal {
    template<class... Ts>
    std::string serialize(const std::tuple<Ts...>& tup) {
        std::string s = "{";
        std::apply([&s](auto&&... args) {
            auto append = [&](const auto& arg) { s += serialize(arg) + ", "; };
            (append(args), ...);
        }, tup);
        if (s.size() > 1)
            s.erase(s.size() - 2); // remove trailing ", "
        return s + "}";
    }
    // Helper function to handle comma-space separator
    template<class Os, class T, class... Ts>
    void tuple_to_stream(Os& os, const T& first, const Ts&... rest) {
        os << serialize(first);
        ((os << ", " << serialize(rest)), ...);
    }
} // namespace internal

template<class... Ts>
std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tup) {
    os << "{";
    std::apply([&os](auto&&... args) {
        internal::tuple_to_stream(os, args...);
        }, tup);
    os << "}";
    return os;
}

///////////////////////////////////////////////////////////////////////////////////////////// zen::stackonly

struct stackonly
{
    static void* operator new(  std::size_t) = delete;
    static void* operator new[](std::size_t) = delete;
    static void  operator delete(  void*)    = delete;
    static void  operator delete[](void*)    = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////// TESTING

#define BEGIN_TEST    zen::log("BEGIN", zen::repeat("-", 50), __func__)
#define BEGIN_SUBTEST zen::log(         zen::repeat("-", 61), __func__)
#define END_TESTS     zen::log("END  ", zen::repeat("-", 50), __func__)

std::atomic<int> TEST_CASE_PASS_COUNT = 0; // atomic in case tests are ever parallelized
std::atomic<int> TEST_CASE_FAIL_COUNT = 0; // atomic in case tests are ever parallelized

bool REPORT_TC_PASS = false; // by default, don't report passes to avoid excessive chatter
bool REPORT_TC_FAIL = true;  // by default, do    report fails (should be few)

#define ZEN_STATIC_ASSERT(X, M) static_assert(X, "ZEN STATIC ASSERTION FAILED. "#M ": " #X)

// ZEN_EXPECT checks its expression parameter and spits out the expression if it fails.
// The do { } while (0) construct ensures that the macro behaves as a single statement.
// This allows it to be used safely in contexts like if-else statements without braces,
// preventing syntax errors or unexpected behavior due to dangling elses.
// Continues execution regardless of the expectation result.
// Example: ZEN_EXPECT(str == "good");
// Result:  CASE PASS: ...
//     or:  CASE FAIL: ...
#define ZEN_EXPECT(expression) \
    do { \
        if (expression) { \
            if (zen::REPORT_TC_PASS) \
                zen::log(zen::color::green("CASE PASS:"), #expression); \
            ++zen::TEST_CASE_PASS_COUNT; \
        } \
        if (!(expression)) { \
            if (zen::REPORT_TC_FAIL) \
                zen::log(zen::color::red("CASE FAIL:"), __func__, "EXPECTED:", #expression); \
            ++zen::TEST_CASE_FAIL_COUNT; \
        } \
    } while (0)

// ZEN_EXPECT_THROW checks its expression parameter to throw the expression_type exception
// and spits out the expression statement if it encounters another exception type thrown.
// The do { } while (0) construct ensures that the macro behaves as a single statement.
// This allows it to be used safely in contexts like if-else statements without braces,
// preventing syntax errors or unexpected behavior due to dangling elses.
// Example: ZEN_EXPECT_THROW(zen::version vi("bad"), std::invalid_argument);
// Continues execution regardless of the expectation result.
// Result:  CASE PASS: ...
//     or:  CASE FAIL: ...
#define ZEN_EXPECT_THROW(expression, exception_type) \
    do { \
        bool exception_caught{false}; \
        try { \
            expression; \
        } \
        catch (const exception_type&) { \
            exception_caught = true; \
            if (zen::REPORT_TC_PASS) \
                zen::log(zen::color::green("CASE PASS:"), #expression); \
            ++zen::TEST_CASE_PASS_COUNT; \
            break; \
        } \
        catch (...) { \
            exception_caught = true; \
            if (zen::REPORT_TC_FAIL) \
                zen::log(zen::color::red("CASE FAIL:"), __func__, \
                        "EXPECTED `" #expression \
                        "` TO THROW AN EXCEPTION OF TYPE `" #exception_type \
                        "`, BUT IT THROWS ANOTHER TYPE."); \
            ++zen::TEST_CASE_FAIL_COUNT; \
            break; \
        } \
        if (!exception_caught) { \
            if (zen::REPORT_TC_FAIL) \
                zen::log(zen::color::red("CASE FAIL:"), __func__, \
                        "EXPECTED `" #expression \
                        "` TO THROW AN EXCEPTION, BUT IT DOES NOT."); \
            ++zen::TEST_CASE_FAIL_COUNT; \
        } \
    } while(0)

// ZEN_EXPECT_NOTHROW checks its expression parameter to not throw an exception
// and spits out the expression statement if it an exception is, in fact, thrown.
// The do { } while (0) construct ensures that the macro behaves as a single statement.
// This allows it to be used safely in contexts like if-else statements without braces,
// preventing syntax errors or unexpected behavior due to dangling elses.
// Example: ZEN_EXPECT_NOTHROW(no_throw_function());
// Continues execution regardless of the expectation result.
// Result:  CASE PASS: ...
//     or:  CASE FAIL: ...
#define ZEN_EXPECT_NOTHROW(expression) \
    do { \
        bool exception_caught{false}; \
        try { \
            expression; \
        } \
        catch (...) { \
            exception_caught = true; \
            if (zen::REPORT_TC_FAIL) \
                zen::log(zen::color::red("CASE FAIL:"), __func__, \
                        "EXPECTED `" #expression "` NOT TO THROW ANY EXCEPTION, BUT IT DID."); \
            ++zen::TEST_CASE_FAIL_COUNT; \
            break; \
        } \
        if (!exception_caught) { \
            if (zen::REPORT_TC_PASS) \
                zen::log(zen::color::green("CASE PASS:"), #expression); \
            ++zen::TEST_CASE_PASS_COUNT; \
        } \
    } while(0)

///////////////////////////////////////////////////////////////////////////////////////////// COLORS
// Example: zen::print(zen::color::red(str));
// Example: std::cout( zen::color::red(str));
// Result: Red-colored str in the console.
namespace color {
    class color_string {
    public:
        color_string(const std::string_view s, int c) : text(s), code(c) {}
        const std::string text;
        const int /*col*/ code;

        friend std::ostream& operator<<(std::ostream& os, const color_string& cw) {
            os << "\033[" << cw.code << "m" << cw.text << "\033[0m";
            return os;
        }
    };

    color_string nocolor(const std::string_view s) { return color_string(s,  0); }
    color_string red    (const std::string_view s) { return color_string(s, 31); }
    color_string blue   (const std::string_view s) { return color_string(s, 34); }
    color_string green  (const std::string_view s) { return color_string(s, 32); }
    color_string black  (const std::string_view s) { return color_string(s, 30); }
    color_string yellow (const std::string_view s) { return color_string(s, 33); }
    color_string magenta(const std::string_view s) { return color_string(s, 35); }
    color_string cyan   (const std::string_view s) { return color_string(s, 36); }
    color_string white  (const std::string_view s) { return color_string(s, 37); }
}

///////////////////////////////////////////////////////////////////////////////////////////// FILESYSTEM

std::filesystem::path current_path() { return std::filesystem::current_path(); }
std::filesystem::path  parent_path() { return std::filesystem::current_path().parent_path(); }

std::optional<std::filesystem::path>
search_upward(std::string_view name, std::filesystem::path from = std::filesystem::current_path())
{
    while (from.filename() != name) {
        if (from.root_path() == from && name == "/")
            return from;

        // In most file systems, attempting to go to the parent of the root
        // directory returns the root directory itself. Therefore, to avoid
        // potentially infinite loops when the search reaches the root
        // directory and still can't find the specified directory or file
        // handle, we check to see if the parent of 'from' is 'from' itself:
        std::filesystem::path parent = from.parent_path();
        if (from == parent)
            return std::nullopt;

        from = parent;
    }

    if (from.empty())
        return std::nullopt;
        
    return from;
}

std::optional<std::filesystem::path>
search_downward(std::string_view name, std::filesystem::path from = std::filesystem::current_path(), const int depth = 10)
{
    std::queue<std::pair<std::filesystem::path, int>> search_queue;
    search_queue.push({ from, 0 });

    while (!search_queue.empty()) {
        const auto [current_path, current_depth] = search_queue.front();
        search_queue.pop();

        if (current_path.filename() == name)
            return current_path;

        if (current_depth >= depth)
            continue;

        if (std::filesystem::is_directory(current_path))
            for (const auto& entry : std::filesystem::directory_iterator(current_path))
                search_queue.push({ entry.path(), current_depth + 1 });
    }

    return std::nullopt;
}

///////////////////////////////////////////////////////////////////////////////////////////// zen::array

template<class T, size_t N>
class array : public std::array<T, N>, private zen::stackonly
{
public:
    using std::array<T, N>::array; // inherit constructors, has to be explicit

    array(const std::array<T, N>& a) : std::array<T, N>(a) {}

    // Custom constructor to handle initializer list
    array(std::initializer_list<T> init_list)
    {
        std::copy(std::begin(init_list), std::end(init_list), my::begin());
    }

    template<class Pred>
    typename std::enable_if<std::is_invocable_r<bool, Pred, const T&>::value, bool>::type
        contains(Pred p) const
    {
        return std::find_if(my::begin(), my::end(), p) != my::end();
    }
    bool contains(const T& x) const { return std::find(my::begin(), my::end(), x) != my::end(); }

    bool is_empty() const { return my::empty(); }

private:
    using my = array<T, N>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::cloc

// Counts lines of code, use like this:
// 
// zen::cloc cloc(zen::parent_path(), { "datas", "functions", "tests" });
// cloc.count({    ".h",     ".cpp",     ".py" });
// cloc.count({ R"(\.h)", R"(\.cpp)", R"(\.py)" };
// 
// Name is based on the popular utility cloc: https://github.com/AlDanial/cloc
class cloc {
public:
    cloc() // by default initialized with current directory
        : root_(std::filesystem::current_path()), dirs_({ "." }) {}

    // Will search in specified subdirectories of the current directory
    explicit
    cloc(const std::vector<std::string>& dirs)
        : root_(std::filesystem::current_path()), dirs_(dirs) {}

    cloc(const std::filesystem::path& root, const std::vector<std::string>& dirs) 
        : root_(root), dirs_(dirs) {}
 
    // TODO:LATER: This will run cloc on multiple threads, but will require linking with a
    // threading library. Seems to not be worth the cost just for this one. Maybe if
    // there's a more compelling reason to make Kaizen require multithreading.
    // Used like this to run on 10 threads:
    // 
    // zen::cloc cloc;
    // for (int i : zen::in(10))
    //    zen::log(cloc.count_async({ ".h" }).get());
    // 
    //std::future<int> count_async(const std::vector<std::string>& extensions) const {
    //    std::promise<int> prom;
    //    std::future<int> fut = prom.get_future();
    //    std::jthread t([this, extensions, prom = std::move(prom)]() mutable
    //                   {
    //                       prom.set_value(count(extensions));
    //                   });
    //    return fut;
    //}

    int count(const std::vector<std::string>& extensions) const {
        int total_loc = 0;
        for (const auto& dir : dirs_) {
            total_loc += count_in(root_ / dir, extensions);
        }
        return total_loc;
    }

    int count_in(const std::filesystem::path& dir, const std::vector<std::string>& extensions) const {
        int dir_loc = 0;
        for (const auto& file : std::filesystem::recursive_directory_iterator(dir)) {
            if (file.is_regular_file()) {
                const std::string ext = file.path().extension().string();
                if (matches_any(ext, extensions)) {
                    [[maybe_unused]] int loc = dir_loc += count_in_file(file.path());
                    //std::cout << "LOC" << std::setw(5) << loc << " - " << file.path().string() << std::endl; // DEBUG
                }
            }
        }
        return dir_loc;
    }

    int count_in_file(const std::filesystem::path& filename) const {
        std::ifstream file(filename.string());
        std::string line;
        int loc = 0;
        while (std::getline(file, line)) {
            if (std::regex_match(line, std::regex(R"(^\s*[^/\*\\].*\r?$)"))) {
                ++loc;
            }
        }
        return loc;
    }

private:
    bool matches_any(const std::string& ext, const std::vector<std::string>& extensions) const {
        for (const auto& pattern : extensions) {
            if (std::regex_match(ext, std::regex(pattern))) {
                return true;
            }
        }
        return false;
    }

private:
	std::filesystem::path	 root_; // project root
	std::vector<std::string> dirs_; // where to count
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::cmd_args

// Usage:
// int main(argc, argv)
// ...
// zen::cmd_args        cmd_args(argv, argc);
// const bool verbose = cmd_args.accept("-verbose").is_present();
// const bool ignore  = cmd_args.accept("-ignore" ).is_present();

// TODO: Enhance with support for:
// - Help strings
class cmd_args {
public:
    cmd_args() : argv_(nullptr), argc_(0) {}

    cmd_args(const char* const* argv, int argc)
        : argv_(argv), argc_(argc)
    {
        // Check for consistency between argv and argc for
        // the rare cases when they do not come from main()
        if (argc < 0) {
            throw std::invalid_argument("CONSTRUCTOR ARGUMENT argc MUST BE NON-NEGATIVE");
        }
        for (int i = 0; i < argc; ++i) {
            if (argv[i] == nullptr) {
                throw std::invalid_argument("CONSTRUCTOR ARGUMENT argv CONTAINS nullptr ELEMENT(S)");
            }
        }
    }

    auto& accept(const std::string& arg)
    {
        if (arg.empty())
            return *this; // reject accept("") calls

        if (std::find(std::begin(args_accepted_),
                      std::end(  args_accepted_), arg)
                   == std::end(  args_accepted_))
            args_accepted_.push_back(arg);
        return *this;
    }

    // Returns true if either the provided argument 'a' or the last argument added by accept()
    // is present in the command line (with which the program was presumably launched)
    bool is_present(const std::string& arg = "") const
    {
        if (arg.empty())
            return args_accepted_.empty() ? false : is_present(args_accepted_.back());

        for (int i = 0; i < argc_; ++i)
            if (arg == arg_at(i))
                return true;

        return false;
    }

    auto get_options(const std::string& arg) const
    {
        std::vector<std::string> options;

        int idx = find(arg);
        if (idx >= argc_)
            return options; // as empty

        // Collect all non-dashed strings that follow arg as its options
        // Example: --copy from/some/dir to/some/dir -verbose
        //                 ^^^^^^^^^^^^^ ^^^^^^^^^^^
        for (int i = idx + 1; i < argc_; ++i)
        {
            const std::string& ai = arg_at(i);
            if (ai[0] == '-')
                break; // stop collecting when a new dashed argument is encountered

            options.push_back(ai);
        }

        return options;
    }

    std::string arg_at(const int n) const
    {
        if (0 <= n && n < argc_)
            return argv_[n];
        return ""; // signals non-existence
    }

    std::string first() const { return arg_at(0); }
    std::string  last() const { return arg_at(argc_ - 1); }

    std::size_t count_accepted() const { return args_accepted_.size(); }

    int find(const std::string& arg = "") const
    {
        for (int i = 0; i < argc_; ++i)
            if (arg_at(i) == arg)
                return i;

        return argc_; // the end, signals 'not found'
    }

private:
    using arguments = std::vector<std::string>;

    const char* const* argv_;
    const int          argc_;
    arguments          args_accepted_;
};

///////////////////////////////////////////////////////////////////////////////////////////// CONCEPTS

// ------------------------------------------------------------------------------------------ HasEmpty

#if __cpp_concepts >= 202002L
    // Check if a type T has an empty member function
    template <class T>
    concept HasEmpty = requires(T x) {
        { x.empty() } -> std::same_as<bool>;
    };

    template <class T> concept has_empty_v = HasEmpty<T>;
#else // use SFINAE if concepts are not available (pre-C++20)
    template <class T, class = void> struct has_empty : std::false_type {};

    template <class T>
    struct has_empty<T,
        std::void_t<decltype(std::declval<T&>().empty())>
    > : std::true_type {
        static_assert(std::is_same_v<decltype(std::declval<T&>().empty()), bool>, "empty() MUST RETURN bool");
    };

    template <class T> constexpr bool has_empty_v = has_empty<T>::value;
#endif

// ------------------------------------------------------------------------------------------ Iterable

#if __cpp_concepts >= 202002L
    // Check if a type is iterable
    template <class T>
    concept Iterable = requires(T x) {
       *std::begin(x); // has begin and can be dereferenced
        std::end(x);   // has an end
    };
    template <class T> concept is_iterable_v = Iterable<T>;
#else // use SFINAE if concepts are not available (pre-C++20)
    template <class T, class = void> struct is_iterable : std::false_type {};

    template <class T>
    struct is_iterable<T,
        std::void_t<
            decltype(*std::begin(std::declval<T&>())), // has begin and can be dereferenced
            decltype( std::end(  std::declval<T&>()))  // has an end
        >
    > : std::true_type {};

    template <class T> constexpr bool is_iterable_v = is_iterable<T>::value;
#endif

// ------------------------------------------------------------------------------------------ Addable

#if __cpp_concepts >= 202002L
    template <class T>
    concept Addable = requires(T x, T y) { x + y; };
    template <class T> concept is_addable_v = Addable<T>;
#else
    template <class T, class = void> struct is_addable : std::false_type {};

    template <class T>
    struct is_addable<T,
        std::void_t<decltype(std::declval<T>() + std::declval<T>())>
    > : std::true_type {};

    template <class T> constexpr bool is_addable_v = is_addable<T>::value;
#endif

// ------------------------------------------------------------------------------------------ Arithmetic

#if __cpp_concepts >= 202002L
    template <class T>
    concept Arithmetic = std::is_arithmetic<T>::value;
    template <class T> concept is_arithmetic_v = Arithmetic<T>;
#else
    template <class T> struct is_arithmetic : std::is_arithmetic<T> {};
    template <class T> constexpr bool is_arithmetic_v = is_arithmetic<T>::value;
#endif

// ------------------------------------------------------------------------------------------ Resizable

#if __cpp_concepts >= 202002L
    template <class T>
    concept Resizable = requires(T x, size_t n) {
        x.resize(n); // has a resize method
        { x.size() } -> std::same_as<size_t>; // has a size method returning size_t
    };
    template <class T> concept is_resizable_v = Resizable<T>;
#else
    template <class T, class = void> struct is_resizable : std::false_type {};

    template <class T>
    struct is_resizable<T,
        std::void_t<
            decltype(std::declval<T&>().resize(std::declval<size_t>())), // has a resize method
            decltype(std::declval<T&>().size())                          // has a size method
        >
    > : std::true_type {};

    template <class T> constexpr bool is_resizable_v = is_resizable<T>::value;
#endif

// ------------------------------------------------------------------------------------------ EqualityComparable

#if __cpp_concepts >= 202002L
    // Check if a type is equality comparable
    template <class T>
    concept EqualityComparable = requires(T x, T y) {
        { x == y } -> std::same_as<bool>; // can be compared using ==
    };
    template <class T> concept is_equality_comparable_v = EqualityComparable<T>;
#else // use SFINAE if concepts are not available (pre-C++20)
    template <class T, class = void> struct is_equality_comparable : std::false_type {};

    template <class T>
    struct is_equality_comparable<T,
        std::void_t<
            decltype(std::declval<T&>() == std::declval<T&>()) // can be compared using ==
        >
    > : std::true_type {};

    template <class T> constexpr bool is_equality_comparable_v = is_equality_comparable<T>::value;
#endif

// ------------------------------------------------------------------------------------------ is_string_like

template<class T>
constexpr bool is_string_like() {
    return std::is_convertible<T, std::string>::value
        || std::is_convertible<T, const char*>::value;
}

///////////////////////////////////////////////////////////////////////////////////////////// zen::deque

template<class T, class A = std::allocator<T>>
class deque : public std::deque<T, A>, private zen::stackonly
{
public:
    using std::deque<T, A>::deque; // inherit constructors, has to be explicit

    deque(const std::deque<T, A>& d) : std::deque<T, A>(d) {}

    template<class Pred>
    typename std::enable_if<std::is_invocable_r<bool, Pred, const T&>::value, bool>::type
         contains(Pred p) const
    {
        return std::find_if(my::begin(), my::end(), p) != my::end();
    }
    bool contains(const T& x) const { return std::find(my::begin(), my::end(), x) != my::end(); }

    bool is_empty() const { return my::empty(); }

private:
    using my = deque<T, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::deref

namespace internal {

// Base case: stop dereferencing
template <typename T, typename = void>
struct deref_recursive {
    static T& deref(T& x) { return x; }
};

// Recursive case: continue dereferencing
template <typename T>
struct deref_recursive<T, std::void_t<decltype(*std::declval<T&>())>> {
    static auto deref(T& x) -> decltype(deref_recursive<std::remove_reference_t<decltype(*x)>>::deref(*x)) {
        return                          deref_recursive<std::remove_reference_t<decltype(*x)>>::deref(*x);
    }
};

} // namespace internal

// Main deref function
template <typename T>
auto deref(T& x) -> decltype(internal::deref_recursive<T>::deref(x)) {
    return                   internal::deref_recursive<T>::deref(x);
}

// Forward declarations
std::string quote(const std::string_view s);

///////////////////////////////////////////////////////////////////////////////////////////// zen::file

class file : public std::fstream {
public:
    file(const std::filesystem::path& path)
        : std::fstream(path), filepath_(path)
    {
        if (!my::is_open()) {
            throw std::runtime_error("ERROR OPENING FILE: " + zen::quote(path.string()));
        }
    }

    ~file() {
        if (my::is_open()) {
            my::close();
        }
    }

    class iterator {
    public:
        iterator(file& is, bool end_marker = false)
            : input_{ is }, end_marker_{ end_marker }
        {
            if (!end_marker_) {
                input_.clear();
                input_.seekg(0, std::ios::beg);
                this->operator++();
            }
        }

        bool operator!=(const iterator& it) const {
            return it.end_marker_ != end_marker_;
        }

        const std::string& operator*() const {
            return line_;
        }

        iterator& operator++() {
            if (input_.eof())
                end_marker_ = true;
            else
                std::getline(input_, line_, '\n');

            return *this;
        }

    private:
        file&        input_;
        bool         end_marker_{ false };
        std::string  line_;
    };

    auto begin() { return iterator{ *this }; }
    auto end()   { return iterator{ *this, true }; }

    std::string getline(int nth)
    {
        auto it = begin();
        while (--nth > 0 && it != end()) {
            ++it;
        }

        if (nth != 0)
            throw std::out_of_range("REACHED END OF FILE: " + zen::quote(filepath_.string()));

        return *it;
    }

private:
    // TODO: Dynamically cache lines that are read the first time?
    const std::filesystem::path& filepath_;

    using my = std::fstream;
};

namespace literals::path {

std::filesystem::path operator ""_path(const char* str, std::size_t length)
{
    return std::filesystem::path(std::string(str, length));
}

} // namespace literals::path

///////////////////////////////////////////////////////////////////////////////////////////// zen::forward_list

template<class T, class A = std::allocator<T>>
class forward_list : public std::forward_list<T, A>, private zen::stackonly
{
public:
    using std::forward_list<T, A>::forward_list; // inherit constructors, has to be explicit

    forward_list(const std::forward_list<T, A>& x) : std::forward_list<T, A>(x) {}

    template<class Pred>
    typename std::enable_if<std::is_invocable_r<bool, Pred, const T&>::value, bool>::type
        contains(Pred p) const
    {
        return std::find_if(my::begin(), my::end(), p) != my::end();
    }
    bool contains(const T& x) const { return std::find(my::begin(), my::end(), x) != my::end(); }

    bool is_empty() const { return my::empty(); }

private:
    using my = forward_list<T, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::in

// Declarative range-for loop. Note that apart from an intuitive
// reading, "in" can also be thought of standing for "interval".
// Example: for (int i : zen::in(5))         // from  0 to  5
// Example: for (int i : zen::in(1, 10))     // from  1 to 10
// Example: for (int i : zen::in(10, 1, -1)) // from 10 to  1, step -1
class in {
public:
    in(int end)
        : begin_(0), end_(end), step_(1) {}

    in(int begin, int end, int step = 1)
        : begin_(begin), end_(end), step_(step) {}

    class iterator {
    public:
        iterator(int n = 0, int step = 1) : n_(n), step_(step) {}
        iterator& operator++() { n_ += step_; return *this; }
        const int& operator*()             const { return n_; }
        bool operator!=(const iterator& x) const {
            return (step_ > 0) ? (n_ < x.n_) : (n_ > x.n_);
        }
    private:
        int n_;
        int step_;
    };

    iterator begin() const { return iterator(begin_, step_); }
    iterator end()   const { return iterator(end_,   step_); }

private:
    int begin_;
    int end_;
    int step_;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::list

template<class T, class A = std::allocator<T>>
class list : public std::list<T, A>, private zen::stackonly
{
public:
    using std::list<T, A>::list; // inherit constructors, has to be explicit

    list(const std::list<T, A>& x) : std::list<T, A>(x) {}

    template<class Pred>
    typename std::enable_if<std::is_invocable_r<bool, Pred, const T&>::value, bool>::type
        contains(Pred p) const
    {
        return std::find_if(my::begin(), my::end(), p) != my::end();
    }
    bool contains(const T& x) const { return std::find(my::begin(), my::end(), x) != my::end(); }

    bool is_empty() const { return my::empty(); }

private:
    using my = list<T, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::map

template<class K, class V, class C = std::less<K>, class A = std::allocator<std::pair<const K, V>>>
class map : public std::map<K, V, C, A>, private zen::stackonly
{
public:
    using std::map<K, V, C, A>::map; // inherit constructors, has to be explicit

    map(const std::map<K, V, C, A>& m) : std::map<K, V, C, A>(m) {}

    template<class Kx, class Vx, class Cx, class Ax>
    map(const std::map<Kx, Vx, Cx, Ax>& m) : std::map<K, V, C, A>(m.begin(), m.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = map<K, V, C, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::multimap

template<class K, class V, class C = std::less<K>, class A = std::allocator<std::pair<const K, V>>>
class multimap : public std::multimap<K, V, C, A>, private zen::stackonly
{
public:
    using std::multimap<K, V, C, A>::multimap; // inherit constructors, has to be explicit

    multimap(const std::multimap<K, V, C, A>& m) : std::multimap<K, V, C, A>(m) {}

    template<class Kx, class Vx, class Cx, class Ax>
    multimap(const std::multimap<Kx, Vx, Cx, Ax>& m) : std::multimap<K, V, C, A>(m.begin(), m.end()) {}

    // std::map::operator[] is not defined, but
    // zen::map::operator[] returns an std::vector
    // composed of values corresponding to the parameter key.
    std::vector<V> operator[](const K& key) {
        auto range = my::equal_range(key);
        std::vector<V> values;
        for (auto it = range.first; it != range.second; ++it) {
            values.push_back(it->second);
        }
        return values;
    }

    bool is_empty() const { return my::empty(); }

private:
    using my = multimap<K, V, C, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::point2d

class point2d : public std::pair<double, double> {
public:
    point2d(double xc = 0.0, double yc = 0.0)    : std::pair<double, double>(xc, yc)       {}
    point2d(const std::pair<double, double>&  p) : std::pair<double, double>(p)            {}
    point2d(      std::pair<double, double>&& p) : std::pair<double, double>(std::move(p)) {}

    // Allows conversions from other arithmetic pair types
    template <class T, class U, typename std::enable_if<
                                         std::is_arithmetic_v<T> &&
                                         std::is_arithmetic_v<U>, int>::type = 0>
    point2d(const std::pair<T, U>& p)
        : std::pair<double, double>(
            static_cast<double>(p.first),
            static_cast<double>(p.second)
        )
    {}

    point2d& operator=(const std::pair<double, double>& p) {
        this->first  = p.first;
        this->second = p.second;
        return *this;
    }

    point2d& operator=(std::pair<double, double>&& p) {
        this->first  = std::move(p.first);
        this->second = std::move(p.second);
        return *this;
    }

    // Returning reference to the member is deliberate since
    // x() and y() are simply meant to be a convenience alias
    // interface to avoid writing '.first' or '.second' 
    constexpr double& x()       { return this->first;  }
    constexpr double& y()       { return this->second; }
    constexpr double  x() const { return this->first;  }
    constexpr double  y() const { return this->second; }

    friend point2d operator+(const point2d& a, const point2d& b) { return point2d(a.x() + b.x(), a.y() + b.y()); }
    friend point2d operator-(const point2d& a, const point2d& b) { return point2d(a.x() - b.x(), a.y() - b.y()); }
    friend point2d operator*(const point2d& a, const double   k) { return point2d(a.x() * k, a.y() * k); }
    friend bool   operator==(const point2d& a, const point2d& b) { return a.x() == b.x() && a.y() == b.y(); }
    friend bool   operator!=(const point2d& a, const point2d& b) { return !(a == b); }
    friend point2d operator/(const point2d& a, double k) {
        if (k != 0) {
            return point2d(a.x() / k, a.y() / k);
        } else {
            throw std::invalid_argument("ATTEMPTED DIVISION BY ZERO FOR point2d");
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::point3d

class point3d : public point2d {
public:
    point3d(double xc = 0.0, double yc = 0.0, double zc = 0.0) : point2d(xc, yc), z_(zc) {}

    point3d(const point2d& p, double zc = 0.0) : point2d(p), z_(zc) {}

    template <class T, class U, class V, typename std::enable_if<
                                                  std::is_arithmetic_v<T> &&
                                                  std::is_arithmetic_v<U> &&
                                                  std::is_arithmetic_v<V>, int>::type = 0>
    point3d(const std::tuple<T, U, V>& p)
        : point2d(std::get<0>(p), std::get<1>(p)),
           z_(static_cast<double>(std::get<2>(p)))
    {}

    point3d& operator=(const std::pair<double, double>& p) {
        point2d::operator=(p);            // inherit behavior for x and y
        z_ = 0;                           // reset to default
        return *this;
    }

    point3d& operator=(std::pair<double, double>&& p) {
        point2d::operator=(std::move(p)); // inherit behavior for x and y
        z_ = 0;                           // reset to default
        return *this;
    }

    // Returning reference to the member is deliberate since
    // x(), y() and z() are simply meant to be a convenience alias
    // interface to avoid writing '->first', '->second' or '->z_'
    constexpr double& z()       { return z_; }
    constexpr double  z() const { return z_; }

    friend point3d operator+(const point3d& a, const point3d& b) { return point3d(a.x() + b.x(), a.y() + b.y(), a.z() + b.z()); }
    friend point3d operator-(const point3d& a, const point3d& b) { return point3d(a.x() - b.x(), a.y() - b.y(), a.z() - b.z()); }
    friend point3d operator*(const point3d& a, const double k)   { return point3d(a.x() * k, a.y() * k, a.z() * k); }
    friend bool   operator==(const point3d& a, const point3d& b) { return a.x() == b.x() && a.y() == b.y() && a.z() == b.z(); }
    friend bool   operator!=(const point3d& a, const point3d& b) { return !(a == b); }

    friend point3d operator/(const point3d& a, double k) {
        if (k != 0) {
            return point3d(a.x() / k, a.y() / k, a.z() / k);
        }
        else {
            throw std::invalid_argument("ATTEMPTED DIVISION BY ZERO FOR point3d");
        }
    }

private:
    double z_;
};

// ------------------------------------------------------------------------------------------ aliases

using point = point2d;

///////////////////////////////////////////////////////////////////////////////////////////// zen::queue

template<class T, class C = std::deque<T>>
class queue : public std::queue<T, C>, private zen::stackonly
{
public:
    using std::queue<T, C>::queue; // inherit constructors, has to be explicit

    queue(const std::queue<T, C>& q) : std::queue<T, C>(q) {}
    
    template<class Iterable>
    queue(const Iterable& c)
    {
        ZEN_STATIC_ASSERT(zen::is_iterable_v<Iterable>, "TEMPLATE PARAMETER EXPECTED TO BE Iterable, BUT IS NOT");

        for (const auto& x : c)
            my::push(x);
    }

    bool is_empty() const { return my::empty(); }

private:
    using my = queue<T, C>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::priority_queue

template<
    class T,
    class C = std::vector<T>,
    class L = std::less<typename C::value_type>
>
class priority_queue : public std::priority_queue<T, C, L>, private zen::stackonly
{
public:
    using std::priority_queue<T, C, L>::priority_queue; // inherit constructors, has to be explicit

    priority_queue(const std::priority_queue<T, C, L>& q) : std::priority_queue<T, C, L>(q) {}

    template<class Iterable>
    priority_queue(const Iterable& c)
    {
        ZEN_STATIC_ASSERT(zen::is_iterable_v<Iterable>, "TEMPLATE PARAMETER EXPECTED TO BE Iterable, BUT IS NOT");

        for (const auto& x : c)
            my::push(x);
    }

    bool is_empty() const { return my::empty(); }

private:
    using my = priority_queue<T, C, L>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::set

template<class K, class C = std::less<K>, class A = std::allocator<K>>
class set : public std::set<K, C, A>, private zen::stackonly
{
public:
    using std::set<K, C, A>::set; // inherit constructors, has to be explicit

    set(const std::set<K, C, A>& u) : std::set<K, C, A>(u) {}

    template<class Kx, class Cx, class Ax>
    set(const std::set<Kx, Cx, Ax>& u) : std::set<K, C, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = set<K, C, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::multiset

template<class K, class C = std::less<K>, class A = std::allocator<K>>
class multiset : public std::multiset<K, C, A>, private zen::stackonly
{
public:
    using std::multiset<K, C, A>::multiset; // inherit constructors, has to be explicit

    multiset(const std::multiset<K, C, A>& u) : std::multiset<K, C, A>(u) {}

    template<class Kx, class Cx, class Ax>
    multiset(const std::multiset<Kx, Cx, Ax>& u) : std::multiset<K, C, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = multiset<K, C, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::stack

template<class T, class C = std::deque<T>>
class stack : public std::stack<T, C>, private zen::stackonly
{
public:
    using std::stack<T, C>::stack; // inherit constructors, has to be explicit

    stack(const std::stack<T, C>& q) : std::stack<T, C>(q) {}
        
    bool is_empty() const { return my::empty(); }

private:
    using my = stack<T, C>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::string

class string : public std::string, private zen::stackonly
{
public:
    using std::string::string;    // inherit constructors,         has to be explicit
    using std::string::operator=; // inherit assignment operators, has to be explicit

    string(const std::string&     s) : std::string(s) {}
    string(const std::string_view s) : std::string(s) {}

#if __cplusplus < 202303L // check pre-C++23, at which point std::string::contains() is standard
    // SFINAE to ensure that this version is only enabled when Pred is callable
    template<class Pred, typename = std::enable_if_t<std::is_invocable_r_v<bool, Pred, char>>>
    bool contains(const Pred& p)            const { return std::find_if(my::begin(), my::end(), p) != my::end(); }
    bool contains(const std::string_view s) const { return find(s) != std::string::npos; }
#endif

    bool is_empty() const { return my::empty(); }

    // std::string s = "[EXTRACTME]"; 
    //                   ^^^^^^^^^
    // Example: s.extract_between("[", "]");
    zen::string extract_between(const std::string_view beg, const std::string_view end) const
    {
        const size_t posBeg = find(beg);
        if (posBeg == std::string::npos) return ""; // signals 'not found'
        const size_t posEnd = find(end, posBeg + 1);
        if (posEnd == std::string::npos) return ""; // signals 'not found'
        return substr(posBeg + 1, posEnd - posBeg - 1);
    }

    zen::string extract_pattern(const std::string& pattern)
    {
        const std::regex regex_pattern(pattern);
        std::smatch match;
        std::string in(my::begin(), my::end());

        if (std::regex_search(in,   match, regex_pattern)) {
            const size_t startPos = match.position(0);
            const size_t length   = match.length(0);

            // Create a sub-string_view using the position and length
            return std::string(my::data() + startPos, length);
        }

        return ""; // signals 'no match'
    }

    zen::string& remove(const std::string& pattern)
    {
        *this = std::regex_replace(*this, std::regex(pattern), std::string(""));
        return *this; // for natural chaining
    }

    auto extract_version()   { return extract_pattern(R"((\d+)\.(\d+)\.(\d+)\.(\d+))"                          ); } // Like "X.Y.Z.B"
    auto extract_date()      { return extract_pattern(R"((\d+\/\d+\/\d+))"                                     ); } // Like "31/12/2021"
    auto extract_email()     { return extract_pattern(R"((\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b))"); }
    auto extract_url()       { return extract_pattern(R"((https?://[^\s]+))"                                   ); }
    auto extract_hashtag()   { return extract_pattern(R"((#\w+))"                                              ); } // Like "#event"
    auto extract_extension() { return extract_pattern(R"((\.\w+$))"                                            ); }

    // Modifying functions
    auto& prefix(const std::string_view s)
    {
        insert(0, s);
        return *this;
    }

    // Behaves like JavaScript's string.replace()
    auto& replace(const std::string& search, const std::string& replacement) {
        const size_t position = std::string::find(search);
        if (position != std::string::npos) {
            std::string::replace(position, search.length(), replacement);
        }
        return *this;
    }

    template <typename Pred>
    auto& replace_if(const std::string& search, const std::string& replacement, Pred predicate)
    {
        if (search.empty()) return *this;

        static_assert(std::is_invocable<Pred, const std::string&>(),
            "TEMPLATE PARAMETER Pred MUST BE CALLABLE WITH const std::string&, BUT IS NOT");
        static_assert(std::is_same_v<std::invoke_result_t<Pred, const std::string&>, bool>,
            "TEMPLATE PARAMETER Pred MUST RETURN bool, BUT DOES NOT");

        const size_t position = std::string::find(search);
        if (position != std::string::npos && predicate(*this)) {
            std::string::replace(position, search.length(), replacement);
        }
        return *this;
    }

    // Behaves like JavaScript's string.replaceAll()
    auto& replace_all(const std::string& search, const std::string& replacement)
    {
        if (search.empty()) return *this;

        size_t pos = 0;
        while ((pos = my::find(search, pos)) != std::string::npos) {
            std::string::replace(pos, search.length(), replacement);
            pos += replacement.length(); // move pos forward by the length of replace to prevent infinite loops
        }
        return *this;
    }

    template <typename Pred>
    auto& replace_all_if(const std::string& search, const std::string& replacement, Pred predicate)
    {
        if (search.empty()) return *this;

        static_assert(std::is_invocable<Pred, const std::string&>(),
            "TEMPLATE PARAMETER Pred MUST BE CALLABLE WITH const std::string&, BUT IS NOT");
        static_assert(std::is_same_v<std::invoke_result_t<Pred, const std::string&>, bool>,
            "TEMPLATE PARAMETER Pred MUST RETURN bool, BUT DOES NOT");

        size_t pos = 0;
        while ((pos = my::find(search, pos)) != std::string::npos) {
            if (predicate(*this)) {
                std::string::replace(pos, search.length(), replacement);
                pos += replacement.length(); // move pos forward by the length of replace to prevent infinite loops
            } else {
                pos += search.length(); // move pos forward by the length of search
            }
        }
        return *this;
    }

    auto& trim_from_last(const std::string_view str)
    {
        *this = substr(0, rfind(str));
        return *this;
    }

    auto& trim()
    {
        // Trim leading and trailing spaces
        my::assign(std::regex_replace(*this, std::regex("^\\s+|\\s+$"), std::string("")));
        return *this; // for natural chaining
    }

    bool is_trimmed()
    {
        return !::isspace(my::front()) &&!::isspace(my::back());
    }

    auto& deflate()
    {
        // Replace any & all multiple spaces with a single space
        my::assign(std::regex_replace(my::trim(), std::regex("\\s+"), " "));
        return *this; // for natural chaining
    }

    bool is_deflated() const
    {
        auto neighbor_spaces = [](char a, char b) { return std::isspace(a) && std::isspace(b); };
        return my::end() == std::adjacent_find(my::begin(), my::end(), neighbor_spaces);
    }

    auto substring(int i1, int i2) const
    {
        const int sz = static_cast<int>(size());

        // If necessary, convert negative indices to positive
        if (i1 < 0) i1 += sz;
        if (i2 < 0) i2 += sz;

        // Clamp indices to valid range
        i1 = std::clamp<int>(i1, 0, sz);
        i2 = std::clamp<int>(i2, 0, sz);

        if (i2 <= i1) {
            return zen::string(""); // empty string signals a negative result and is harmless
        }

        return zen::string(substr(i1, i2 - i1));
    }

    auto& pad_start(size_t target_length, const std::string& pad_string = " ")
    {
        if (pad_string.empty()) return *this;

        const size_t current_length = my::size();
        const size_t total_padding_needed = target_length > current_length ? target_length - current_length : 0;

        // Full pad strings
        const size_t full_pads = total_padding_needed / pad_string.length();

        // Remaining characters
        const size_t remaining = total_padding_needed % pad_string.length();

        // Generate the padding string
        std::string padding;
        for (size_t i = 0; i < full_pads; ++i) {
            padding += pad_string;
        }
        padding += pad_string.substr(0, remaining);

        my::insert(0, padding);

        return *this;
    }

    auto& pad_end(size_t target_length, const std::string& pad_string = " ")
    {
        if (pad_string.empty()) return *this;

        while (my::size() < target_length) {
            // Append as much of pad_string as will fit
            my::append(pad_string, 0, std::min(pad_string.size(), target_length - my::size()));
        }

        return *this;
    }

    auto& capitalize()
    {
        if (is_empty()) return *this;

        if (std::isalpha(front()) && std::islower(front())) {
            my::front() = static_cast<char>(std::toupper(my::front())); // capitalize the first character
        }

        for (size_t i = 1; i < my::size(); ++i) {
            char& c = my::at(i);
            if (std::isalpha(c) && std::isupper(c)) {
                c = static_cast<char>(std::tolower(c));
            }
        }

        return *this;
    }

    auto& to_lower() {
        for (auto& c : *this) {
            if (std::isalpha(c) && std::isupper(c)) {
                c = static_cast<char>(std::tolower(c));
            }
        }
        return *this;
    }

    auto& to_upper() {
        for (auto& c : *this) {
            if (std::isalpha(c) && std::islower(c)) {
                c = static_cast<char>(std::toupper(c));
            }
        }
        return *this;
    }

    auto& center(size_t width, char fillchar = ' ') {
        if (width <= my::size()) return *this;

        const size_t padding = width - my::size();
        const size_t left_padding  = padding / 2;
        const size_t right_padding = padding - left_padding;

        my::insert(0, left_padding, fillchar);
        my::append(right_padding, fillchar);

        return *this;
    }

    bool is_printable() const { return                std::all_of(my::begin(), my::end(), [](auto c) { return std::isprint(c); }); }
    bool is_alnum()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::isalnum(c); }); }
    bool is_alpha()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::isalpha(c); }); }
    bool is_digit()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::isdigit(c); }); }
    bool is_lower()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::islower(c); }); }
    bool is_upper()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::isupper(c); }); }
    bool is_space()     const { return !is_empty() && std::all_of(my::begin(), my::end(), [](auto c) { return std::isspace(c); }); }

    bool is_identifier() const
    {
        if (is_empty())
            return false;

        if (!std::isalpha(front()) && front() != '_')
            return false;

        for (size_t i = 1; i < my::size(); ++i) {
            const char& c = my::at(i);
            if (!std::isalnum(c) && c != '_') {
                return false;
            }
        }

        return true;
    }

    auto& ljust(int width, char fillchar = ' ')
    {
        if (width < 0)
            width = 0; // handle negative width gracefully

        auto uwidth = static_cast<std::string::size_type>(width);
        if (uwidth <= my::size())
            return *this;

        const size_t padding = uwidth - my::size();
        my::append(padding, fillchar);

        return *this;
    }

    auto& rjust(int width, char fillchar = ' ')
    {
        if (width < 0)
            width = 0; // handle negative width gracefully

        auto uwidth = static_cast<std::string::size_type>(width);
        if (uwidth <= my::size())
            return *this;

        const std::string::size_type padding = uwidth - my::size();
        my::insert(0, padding, fillchar);

        return *this;
    }

    auto& rtrim()
    {
        my::erase(
            std::find_if(my::rbegin(), my::rend(),
                [](int c) { return !std::isspace(c); }
            ).base(),
            my::end()
        );
        return *this;
    }

    auto& ltrim()
    {
        my::erase(
            my::begin(),
            std::find_if(my::begin(), my::end(), [](int c) { return !std::isspace(c); })
        );
        return *this;
    }

    auto partition(const std::string& separator) 
    {
        if (separator.empty())
            throw std::invalid_argument("STRING SEPARATOR CANNOT BE EMPTY");

        const std::string_view sv(my::data(), my::size());
        const size_t pos = my::find(separator);

        if (pos == std::string::npos)
            return std::make_tuple(sv, std::string_view(), std::string_view());

        const std::string_view before = sv.substr(0, pos);
        const std::string_view after  = sv.substr(pos + separator.length());
        const std::string_view sep    = sv.substr(pos,  separator.length());

        return std::make_tuple(before, sep, after);
    }

    auto rpartition(const std::string& separator)
    {
        if (separator.empty())
            throw std::invalid_argument("STRING SEPARATOR CANNOT BE EMPTY");

        const std::string_view sv(my::data(), my::size());
        const size_t pos = my::rfind(separator);

        if (pos == std::string::npos)
            return std::make_tuple(sv, std::string_view(), std::string_view());

        const std::string_view before = sv.substr(0, pos);
        const std::string_view after  = sv.substr(pos + separator.length());
        const std::string_view sep    = sv.substr(pos,  separator.length());

        return std::make_tuple(before, sep, after);
    }

    std::vector<zen::string> split(const std::string& separator)
    {
        // TODO: Can be template to support any container
        std::vector<zen::string> result;
        std::string s(*this);
        std::string token;
        size_t pos = 0;
        while ((pos = s.find(separator)) != std::string::npos) {
            token = s.substr(0, pos);
            result.push_back(token);
            s.erase(0, pos + separator.length());
        }
        result.push_back(s);
        return result;
    }

    std::vector<zen::string> split_lines()
    {
        std::vector<zen::string> lines;
        std::istringstream f(*this);
        std::string line;
        while (std::getline(f, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    auto& swapcase()
    {
        for (auto& c : *this) {
            if (std::isalpha(c)) {
                c = std::islower(c) ? static_cast<char>(std::toupper(c)) : static_cast<char>(std::tolower(c));
            }
        }
        return *this;
    }

    bool is_ascii()
    {
        for (char c : *this)
            if (!isascii(c))
                return false;
        return true;
    }

private:
    using my = zen::string;
};

struct string_hash {
    size_t operator()(const zen::string& z) const {
        return std::hash<std::string>()(z);
    }
};

template <class Rep, class Period>
std::string adaptive_duration(const std::chrono::duration<Rep, Period>& d)
{
    using namespace std::chrono;

    auto duration_ns = duration_cast<nanoseconds>(d).count();

    if (duration_ns >= 3600e9) return std::to_string(duration_cast<hours>       (d).count()) + " hours";
    if (duration_ns >=   60e9) return std::to_string(duration_cast<minutes>     (d).count()) + " minutes";
    if (duration_ns >=    1e9) return std::to_string(duration_cast<seconds>     (d).count()) + " seconds";
    if (duration_ns >=    1e6) return std::to_string(duration_cast<milliseconds>(d).count()) + " milliseconds";
    if (duration_ns >=    1e3) return std::to_string(duration_cast<microseconds>(d).count()) + " microseconds";

    return std::to_string(duration_ns) + " nanoseconds";
}

///////////////////////////////////////////////////////////////////////////////////////////// zen::timer

class timer {
public:
    timer() : start_(std::chrono::high_resolution_clock::now()), 
               stop_(std::chrono::high_resolution_clock::now())
    {}

    auto start() { start_ = std::chrono::high_resolution_clock::now(); return *this; }
    auto stop()  {  stop_ = std::chrono::high_resolution_clock::now(); return *this; }

    template<class Duration>
    auto elapsed() const {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<Duration>(now - start_);
    }

    template<class Duration>
    auto duration() const {
        return std::chrono::duration_cast<Duration>(stop_ - start_);
    }

    auto duration_string() const {
        return adaptive_duration(duration<nsec>());
    }

    using nsec = std::chrono::nanoseconds;
    using usec = std::chrono::microseconds;
    using msec = std::chrono::milliseconds;
    using sec  = std::chrono::seconds;
    using min  = std::chrono::minutes;
    using hrs  = std::chrono::hours;
  //using d    = std::chrono::days;   // since C++20
  //using w    = std::chrono::weeks;  // since C++20
  //using m    = std::chrono::months; // since C++20
  //using y    = std::chrono::years;  // since C++20

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
    std::chrono::time_point<std::chrono::high_resolution_clock>  stop_;
};

template<typename Duration = timer::nsec>
auto measure_execution(std::function<void()> operation)
{
    timer t;
    operation();
    t.stop();
    return t.duration<Duration>();
}

///////////////////////////////////////////////////////////////////////////////////////////// zen::unordered_map

template<
    class K,
    class V,
    class H = std::hash<K>,
    class E = std::equal_to<K>,
    class A = std::allocator<std::pair<const K, V>>
>
class unordered_map : public std::unordered_map<K, V, H, E, A>, private zen::stackonly
{
public:
    using std::unordered_map<K, V, H, E, A>::unordered_map; // inherit constructors, has to be explicit

    unordered_map(const std::unordered_map<K, V, H, E, A>& u)
        : std::unordered_map<K, V, H, E, A>(u) {}

    template<class Kx, class Vx, class Hx, class Ex, class Ax>
    unordered_map(const std::unordered_map<Kx, Vx, Hx, Ex, Ax>& u)
        : std::unordered_map<K, V, H, E, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = unordered_map<K, V, H, E, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::unordered_multimap

template<
    class K,
    class V,
    class H = std::hash<K>,
    class E = std::equal_to<K>,
    class A = std::allocator<std::pair<const K, V>>
>
class unordered_multimap : public std::unordered_multimap<K, V, H, E, A>, private zen::stackonly
{
public:
    using std::unordered_multimap<K, V, H, E, A>::unordered_multimap; // inherit constructors, has to be explicit

    unordered_multimap(const std::unordered_multimap<K, V, H, E, A>& u)
        : std::unordered_multimap<K, V, H, E, A>(u) {}

    template<class Kx, class Vx, class Hx, class Ex, class Ax>
    unordered_multimap(const std::unordered_multimap<Kx, Vx, Hx, Ex, Ax>& u)
        : std::unordered_multimap<K, V, H, E, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = unordered_multimap<K, V, H, E, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::unordered_set

template<
    class T,
    class H = std::hash<T>,
    class E = std::equal_to<T>,
    class A = std::allocator<T>
>
class unordered_set : public std::unordered_set<T, H, E, A>, private zen::stackonly
{
public:
    using std::unordered_set<T, H, E, A>::unordered_set; // inherit constructors, has to be explicit

    unordered_set(const std::unordered_set<T, H, E, A>& u)
        : std::unordered_set<T, H, E, A>(u) {}

    template<class Tx, class Hx, class Ex, class Ax>
    unordered_set(const std::unordered_set<Tx, Hx, Ex, Ax>& u)
        : std::unordered_set<T, H, E, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = unordered_set<T, H, E, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::unordered_multiset

template<
    class T,
    class H = std::hash<T>,
    class E = std::equal_to<T>,
    class A = std::allocator<T>
>
class unordered_multiset : public std::unordered_multiset<T, H, E, A>, private zen::stackonly
{
public:
    using std::unordered_multiset<T, H, E, A>::unordered_multiset; // inherit constructors, has to be explicit

    unordered_multiset(const std::unordered_multiset<T, H, E, A>& u)
        : std::unordered_multiset<T, H, E, A>(u) {}

    template<class Tx, class Hx, class Ex, class Ax>
    unordered_multiset(const std::unordered_multiset<Tx, Hx, Ex, Ax>& u)
        : std::unordered_multiset<T, H, E, A>(u.begin(), u.end()) {}

    bool is_empty() const { return my::empty(); }

private:
    using my = unordered_multiset<T, H, E, A>;
};

///////////////////////////////////////////////////////////////////////////////////////////// zen::vector

template<class T, class A = std::allocator<T>>
class vector : public std::vector<T, A>, private zen::stackonly
{
public:
    using std::vector<T, A>::vector; // inherit constructors, has to be explicit

    vector(const std::vector<T, A>& v) : std::vector<T, A>(v) {}

    template<class Pred>
    typename std::enable_if<std::is_invocable_r<bool, Pred, const T&>::value, bool>::type
        contains(Pred p) const
    {
        return std::find_if(my::begin(), my::end(), p) != my::end();
    }

    bool contains(const T& x) const { return std::find(my::begin(), my::end(), x) != my::end(); }
    
    bool is_empty() const { return my::empty(); }

private:
    using my = vector<T, A>;
};

// One Linux system gave the warning: In the GNU C Library, "major" is defined
// by <sys/sysmacros.h>. For historical compatibility, it is currently defined
// by <sys / types.h> as well, but we plan to remove this soon. To use "major",
// include < sys / sysmacros directly. If you did not intend to use a system-defined
// macro "major", you should undefine it after including <sys/types.h>.
// So we undefine them:
#undef major
#undef minor

///////////////////////////////////////////////////////////////////////////////////////////// zen::version
// Example: zen::version v8("8.2.3.4567");
// Example: zen::version v1(1, 2, 3, 4567);
// v1.major() == 1;
// v1.minor() == 2;
// v1.patch() == 3;
// v1.build() == 4567;
class version : public std::array<int, 4> { 
public:
    version(int major, int minor, int patch, int build)
        : std::array<int, 4>{major, minor, patch, build}
    {}

    explicit version(const std::string& text)
    {
        static const std::regex rx_version{R"((\d+)\.(\d+)\.(\d+)\.(\d+))"};
        if (std::smatch sm; std::regex_match(text, sm, rx_version)) {
            at(0) = std::stoi(sm[1]);
            at(1) = std::stoi(sm[2]);
            at(2) = std::stoi(sm[3]);
            at(3) = std::stoi(sm[4]);
        } else {
            throw std::invalid_argument{
                // Any cost of typeid is likely to be dwarfed by the cost of the exception anyway
                std::string(typeid(*this).name()) + " CONSTRUCTOR ARGUMENT STRING DOESN'T MATCH THE EXPECTED M.M.P.B PATTERN."
            };
        }
    }

    constexpr auto major() const { return at(0); }
    constexpr auto minor() const { return at(1); }
    constexpr auto patch() const { return at(2); }
    constexpr auto build() const { return at(3); }
};

std::ostream& operator<<(std::ostream& os, const version& v)
{
    return os << v.major() << '.' << v.minor() << '.' << v.patch() << '.' << v.build();
}

namespace literals::version {

// Example: auto v7 = "7.6.5.4321"_version;
zen::version operator""_version(const char* text, size_t)
{
    return zen::version{text};
}

} // namespace literals::version

///////////////////////////////////////////////////////////////////////////////////////////// USEFUL MISC

// Repeats a string patterns.
// This is the symmetrical complement of repeat(int, str).
// Example: repeat("*", 10);
// Result:  "**********"
zen::string repeat(const std::string_view s, const int n) {
    std::string result;
    for (int i = 0; i < n; i++) {
        result += s;
    }
    return result;
}

// This is the symmetrical complement of repeat(str, int).
// Repeats a string patterns.
// Example: repeat(10, "*");
// Result:  "**********"
zen::string repeat(const int n, const std::string_view s) {
    std::string result;
    for (int i = 0; i < n; i++) {
        result += s;
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////// MAIN UTILITIES

// Example: random_int();
// Result: A random integer between [min, max)
template<class T = int>
T random_int(const T min = 0, const T max = 10) {
    // Reasons why the std::random_device and the std::mt19937 are 'static' below:
    // ---------------------------------------------------------------------------------------------------------------
    // 1. Initialization Efficiency:
    // Random devices and generators often involve some computational cost due to entropy gathering, seeding, generator
    // initialization and good old algorithmic complexity. By declaring them as static, they are initialized only once,
    // the first time the function is called. Subsequent calls to to this function reuse the existing instances, avoiding the overhead.
    // ---------------------------------------------------------------------------------------------------------------
    // 2. State Preservation:
    // Random number generators like std::mt19937 maintain an internal state that evolves as numbers are generated.
    // This state determines the sequence of random numbers produced. By making the std::mt19937 object static,
    // the state is preserved across calls to this function, ensuring a proper random sequence. If the std::mt19937
    // object were reinitialized on every call, it might lead to repeated or patterned sequences, undermining the randomness.
    // ---------------------------------------------------------------------------------------------------------------
    // 3. Thread Safety Considerations:
    // Declaring these variables as static within a function means they are shared across all calls to that function
    // within the program, regardless of where it's called from. This could potentially raise thread-safety issues if the
    // function is called simultaneously from multiple threads. However, in most common usage scenarios where thread
    // safety is not a concern, using static variables for this purpose is fine.
    // ---------------------------------------------------------------------------------------------------------------
    // 4. Resource Management
    // Since the std::random_device and the std::mt19937 object are static, they are not destroyed when the function returns,
    // but rather when the program ends. This ensures that the same random device and generator instances are reused
    // throughout the lifetime of the program, optimizing resource management.
    // ---------------------------------------------------------------------------------------------------------------
    // 5. Avoiding Repetition
    // If we didn't make these objects static, and the random number generator was reseeded with the same or similar
    // seeds (which might happen if the function is called in quick succession), you might get the same or similar random
    // numbers in different calls. Making these static ensures a more varied and truly random sequence.
    // ---------------------------------------------------------------------------------------------------------------
    // The 'static' below is for reasons outlined above.
    static std::random_device        rd;
    static std::mt19937              gen(rd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

// Very often all we want is a dead simple way of quickly
// generating a container filled with some random numbers.
// Example: std::vector<int> v;
//          zen::generate_random(v);
// Result: A vector of size 10 with random integers between [min, max)
template<class Iterable>
void generate_random(Iterable& c, int size = 10) // TODO: Maybe generalize this to make it work with all containers
{
    ZEN_STATIC_ASSERT(zen::is_iterable_v< Iterable>, "TEMPLATE PARAMETER EXPECTED TO BE Iterable, BUT IS NOT");
    ZEN_STATIC_ASSERT(zen::is_resizable_v<Iterable>, "TEMPLATE PARAMETER EXPECTED TO BE RESIZABLE, BUT IS NOT");

    if (std::empty(c))
        c.resize(size);

    std::generate(std::begin(c), std::end(c), [&]() { return random_int(10, 99); });
}

// Over the years it has become clear that the standard member
// function empty() that lacks an 'is_' prefix is confusing to
// non-familiar users due to its ambiguity as a noun and a verb.
// Example: zen::is_empty(c); // c is any iterable container
template<class HasEmpty>
bool is_empty(const HasEmpty& c)
{
    ZEN_STATIC_ASSERT(zen::has_empty_v<HasEmpty>, "TEMPLATE PARAMETER EXPECTED TO HAVE empty(), BUT DOES NOT");
    return c.empty();
}

// TODO: Think of a way to use the Addable concept in addition
// to Iterable that will not make the resulting code too ugly.
template<class Iterable>
auto sum(const Iterable& c)
{
    ZEN_STATIC_ASSERT(is_iterable_v<Iterable>,         "TEMPLATE PARAMETER EXPECTED TO BE Iterable, BUT IS NOT");
    ZEN_STATIC_ASSERT(is_addable_v<decltype(*std::begin(c))>, "ELEMENT TYPE EXPECTED TO BE Addable, BUT IS NOT");

    if (c.empty()) {
        return decltype(*std::begin(c)){}; // zero-initialized value for empty containers
    }

    // By initializing 'sum' to the first element of the collection and not just the tempting 0,
    // this function makes fewer assumptions about the type it's working with, thereby making
    // this function more robust and generic since we're dealing with arbitrary addable types
    // (which could be complex numbers, matrices, etc.).
    auto sum = *std::begin(c);
    for (auto it = std::next(std::begin(c)); it != std::end(c); ++it) {
        sum += *it;
    }

    return sum;
}

template<class Iterable, class EqualityComparable>
auto count(const Iterable& c, const EqualityComparable& x)
{
    ZEN_STATIC_ASSERT(is_iterable_v<Iterable>,
        "TEMPLATE PARAMETER Iterable EXPECTED TO BE ITERABLE, BUT IS NOT");
    ZEN_STATIC_ASSERT(is_equality_comparable_v<EqualityComparable>,
        "TEMPLATE PARAMETER EqualityComparable EXPECTED TO BE EqualityComparable, BUT IS NOT");

    size_t count = 0;
    for (auto it = std::begin(c); it != std::end(c); ++it) {
        if (*it == x)
            ++count;
    }

    return count;
}

template<class Iterable, class Pred>
auto count_if(const Iterable& c, Pred p)
{
    using T = decltype(*std::begin(c));
    ZEN_STATIC_ASSERT(is_iterable_v<Iterable>,
        "TEMPLATE PARAMETER Iterable EXPECTED TO BE ITERABLE, BUT IS NOT");
    ZEN_STATIC_ASSERT((std::is_invocable_r<bool, Pred, const T&>::value),
        "TEMPLATE PARAMETER Predicate NOT APPLICABLE TO ELEMENT TYPE");

    size_t count = 0;
    for (auto it = std::begin(c); it != std::end(c); ++it) {
        if (p(*it))
            ++count;
    }

    return count;
}

///////////////////////////////////////////////////////////////////////////////////////////// LPS (Log, Print, String)
// 
// Printing and logging in Kaizen follows the LPS principle of textual visualization.
// The LPS principle: from string to print to log. This means that:
// 1. to_string() - is the transformation of an object into a string
// 2. print()     - uses to_string() to output the object (as a string)
// 3. log()       - uses print() and adds any formatting, new lines at the end, etc.

// ------------------------------------------------------------------------------------------ stringify

// Converts most of the widely used data types to a string.
// Example: std::vector<int> v = {1, 3, 3};
// Example: to_string(vec) Result: [1, 2, 3]
// Example: to_string(42)  Result: "42"
template<class T>
zen::string to_string(const T& x) {
    std::stringstream ss;
    
    // First check for string-likeness so that zen::pring("abc") prints "abc"
    // and not [a, b, c] as a result of considering strings as iterable below
    if constexpr (is_string_like<T>()) {
        return x;
    } else if constexpr (is_iterable_v<T>) {
        ss << "[";
        auto it = std::begin(x);
        if (it != std::end(x)) {
            if constexpr (is_string_like<decltype(*it)>())
                ss << quote(to_string(*it++));
            else
                ss << to_string(*it++);              // recursive call to handle nested iterables
        }
        for (; it != std::end(x); ++it) {
            if constexpr (is_string_like<decltype(*it)>())
                ss << ", " << quote(to_string(*it)); // recursive call to handle nested iterables
            else
                ss << ", " << to_string(*it);        // recursive call to handle nested iterables
        }
        ss << "]";
    } else { // not iterable, single item
        ss << x;
    }
    return ss.str();
}

// Recursive variadic template to handle multiple arguments
template<class T, class... Args>
inline zen::string to_string(const T& x, const Args&... args) {
    return to_string(x) + " " + to_string(args...);
}
// Base case for the recursive calls
inline zen::string to_string() { return ""; }

// ------------------------------------------------------------------------------------------ print

// Function to handle individual item printing
template <class T>
void print(const T& x)
{
    std::cout << to_string(x);
}

// Generic, almost Python-like print(). Works like this:
// print("Hello", "World", vec, 42); // Output: Hello World [1, 2, 3] 42
// print("Hello", "World", 24, vec); // Output: Hello World 24 [1, 2, 3]
// print("Hello", vec, 42, "World"); // Output: Hello [1, 2, 3] 42 World
template <class T, class... Args>
void print(T x, Args... args) {
    print(x);
    if constexpr (sizeof...(args) != 0) {
        std::cout << " ";
    }
    print(args...);
}
// Base case for the recursive calls
inline void print() {}

// ------------------------------------------------------------------------------------------ log

// Handles logging a single item
template <class T>
void log(const T& x)
{
    print(x, '\n');
}

// Generic, almost Python-like log(). Works similar to the print() function but adds std::endl
template <class T, class... Args>
void log(T x, Args... args) {
    print(x, args...);
    std::cout << std::endl;
}
// Base case for the recursive calls
inline void log() {}

///////////////////////////////////////////////////////////////////////////////////////////// COMPOSITES

// Following are some of the most common data types defined in
// pretty much all C++ projects that use the types on the right.
// The name 'composites' is chosen by analogy with composite materials.

template<
    class T,
    class H = std::hash<T>,
    class E = std::equal_to<T>,
    class A = std::allocator<T>
>
using hash_set = zen::unordered_set<T, H, E, A>;

template<
    class T,
    class H = std::hash<T>,
    class E = std::equal_to<T>,
    class A = std::allocator<T>
>
using hash_multiset = zen::unordered_multiset<T, H, E, A>;

template<
    class K,
    class V,
    class H = std::hash<K>,
    class E = std::equal_to<K>,
    class A = std::allocator<std::pair<const K, V>>
>
using hash_map = zen::unordered_map<K, V, H, E, A>;

template<
    class K,
    class V,
    class H = std::hash<K>,
    class E = std::equal_to<K>,
    class A = std::allocator<std::pair<const K, V>>
>
using hash_multimap = zen::unordered_multimap<K, V, H, E, A>;

// Composite names
using stringlist = zen::list<  zen::string>;
using stringvec  = zen::vector<zen::string>;
using integers   = zen::vector<int>;
using floats     = zen::vector<float>;
using reals      = zen::vector<double>;
using keyval     = zen::map<zen::string, zen::string>;
using points2d   = zen::vector<zen::point2d>;
using points3d   = zen::vector<zen::point3d>;

// Composite name aliases
using dictionary = keyval;
using strings    = stringvec;
using points     = points2d;
using ints       = integers;

} // namespace zen