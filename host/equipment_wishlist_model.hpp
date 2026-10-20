#pragma once
#include <array>
#include <compare>
#include <deque>
#include <iosfwd>
#include <string>
#include <vector>
namespace portablekit::fu::wishlist {
constexpr unsigned item_limit=2048;
using Inventory=std::array<unsigned,item_limit>;
struct Key { unsigned operation{},category{},recipe{},type{},equipment{}; auto operator<=>(const Key &) const = default; };
struct Material { unsigned item{},required{}; auto operator<=>(const Material &) const = default; };
struct Recipe { Key key; std::string name; std::vector<Material> materials; };
struct Requirement { unsigned item{},required{},owned{},missing{}; };
struct Notice { bool complete{}; unsigned item{},quantity{}; std::string equipment; };
class List {
    struct Entry { Recipe recipe; bool ready{}; };
    std::vector<Entry> entries_;
    Inventory previous_{};
    bool initialized_{},quest_{};
    std::vector<Notice> pending_;
public:
    bool contains(const Key &) const;
    bool contains_name(const std::string &) const;
    std::vector<Recipe> recipes() const;
    bool pin(Recipe);
    bool unpin(const Key &);
    std::vector<Requirement> checklist(const Key &) const;
    void observe(const Inventory &,bool quest);
    void announce_ready();
    std::vector<Notice> take_notices();
    void write(std::ostream &) const;
    bool read(std::istream &);
    std::size_t size() const {return entries_.size();}
};
struct Card { Notice notice; double age{}; float y{}; unsigned long long id{}; };
struct Motion { float alpha{},offset{}; };
Motion motion(double age);
// At most three cards. New arrivals enter one at a time; expiry frees a slot.
class Notifications {
    std::deque<Notice> waiting_;
    std::vector<Card> visible_;
    double since_start_{1};
    unsigned peak_{};
    unsigned long long serial_{};
public:
    void push(Notice);
    void advance(double seconds);
    void clear();
    const std::vector<Card> &cards() const {return visible_;}
    std::size_t queued() const {return waiting_.size();}
};
}
