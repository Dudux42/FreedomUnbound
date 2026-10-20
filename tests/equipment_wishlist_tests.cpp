#include "wishlist_scene.hpp"
#include "wishlist_feedback.hpp"
#include "equipment_wishlist_model.hpp"
#include <cmath>
#include <algorithm>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <cstdio>
using namespace portablekit::fu::wishlist;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"wishlist regression at line %u\n",at.line());throw std::runtime_error("wishlist regression");}
}
Recipe recipe(unsigned id,std::string name,unsigned required=3) {
    return {{0,1,id,5,id},std::move(name),{{42,required}}};
}
int main() {
    List list;Inventory owned{};list.observe(owned,false);
    auto a=recipe(1,"Iron Sword"),b=recipe(2,"Iron Hammer");
    check(list.pin(a) && list.pin(b) && !list.pin(a));
    auto needed=list.checklist(a.key);check(needed.size()==1 && needed[0].missing==3);
    list.observe(owned,true);check(list.take_notices().empty());
    // Three shared materials make both recipes ready independently, although
    // crafting both would require six. Each equipment receives its own notice.
    owned[42]=3;list.observe(owned,true);auto notices=list.take_notices();
    check(notices.size()==3 && !notices[0].complete && notices[0].quantity==3);
    check(notices[1].equipment=="Iron Sword" && notices[2].equipment=="Iron Hammer");
    check(list.checklist(b.key)[0].missing==0);
    list.observe(owned,true);check(list.take_notices().empty());
    owned[42]=4;list.observe(owned,true);notices=list.take_notices();check(notices.size()==1 && !notices[0].complete);
    // Spending materials at the smithy rearms both recipes for the next quest.
    owned[42]=1;list.observe(owned,false);check(list.take_notices().empty());
    list.observe(owned,true);check(list.take_notices().empty());
    owned[42]=3;list.observe(owned,true);check(list.take_notices().size()==3);
    check(list.unpin(a.key) && !list.unpin(a.key) && !list.contains(a.key));
    owned[42]=0;list.observe(owned,true);owned[42]=3;list.observe(owned,true);
    notices=list.take_notices();check(notices.size()==2 && notices[1].equipment=="Iron Hammer");
    // Pre-owned inventory and quest-entry reconciliation never create pickups.
    List loaded;check(loaded.pin(a));owned[42]=100;loaded.observe(owned,true);check(loaded.take_notices().empty());
    owned[101]=1;loaded.observe(owned,true);check(loaded.take_notices().empty());
    loaded.observe(owned,false);loaded.announce_ready();
    notices=loaded.take_notices();check(notices.size()==1 && notices[0].complete && notices[0].equipment==a.name);
    loaded.observe(owned,false);check(loaded.take_notices().empty());
    owned[42]=0;loaded.observe(owned,false);loaded.announce_ready();check(loaded.take_notices().empty());
    auto duplicate=recipe(3,"Duplicate materials");duplicate.materials={{42,2},{42,3},{43,1}};
    check(list.pin(duplicate));check(list.checklist(duplicate.key).size()==2 && list.checklist(duplicate.key)[0].required==5);
    auto upgrade=a;upgrade.key.operation=1;check(list.pin(upgrade)); // Distinct upgrade route.
    std::stringstream saved;list.write(saved);List restored;check(restored.read(saved) && restored.size()==3);
    std::stringstream again;restored.write(again);check(again.str()==saved.str());
    for(const auto bad:{"", "FUWISHLIST 9\n0\n", "FUWISHLIST 1\n10001\n", "FUWISHLIST 1\n1\n0 0 1 6 1 \"bad\" 1 2048 1\n"}) {
        std::stringstream in(bad);check(!restored.read(in) && restored.size()==3);
    }
    auto invalid=recipe(8,"Invalid");invalid.materials[0].item=2048;check(!list.pin(invalid));
    check(motion(0).alpha==0 && motion(0).offset>0);
    check(motion(.3).alpha==1 && motion(.3).offset==0);
    check(motion(4.85).alpha>0 && motion(4.85).alpha<1 && motion(4.85).offset<0);
    check(motion(5).alpha==0);
    Notifications queue;for(unsigned i=0;i<6;i++)queue.push({true,0,0,std::to_string(i)});
    queue.advance(0);check(queue.cards().size()==1 && queue.queued()==5);
    const auto first=queue.cards()[0].id;
    queue.advance(.1);check(queue.cards().size()==1);
    for(unsigned i=0;i<9;i++)queue.advance(.1);
    check(queue.cards().size()==3 && queue.queued()==3 && queue.cards()[0].y<queue.cards()[1].y);
    bool replaced=false;unsigned maximum=0;
    for(unsigned i=0;i<120;i++) {
        queue.advance(.1);maximum=std::max(maximum,static_cast<unsigned>(queue.cards().size()));
        if(!queue.cards().empty() && queue.cards()[0].id!=first)replaced=true;
    }
    check(maximum==3 && replaced && queue.cards().empty() && queue.queued()==0);
    queue.push({});queue.advance(.1);queue.clear();check(queue.cards().empty() && !queue.queued());
    SceneGate scene;
    check(!blocks_scene(false,0,29,false)); // Normal village lighting gradient.
    check(!blocks_scene(false,29,29,false));
    check(blocks_scene(false,15,15,true) && !blocks_scene(false,0,0,true));
    check(blocks_scene(true,255,255,false));
    check(!scene.visible(true,0));
    scene.world(false,1);check(!scene.visible(true,1)); // Quest world cannot announce a village return.
    scene.world(true,2);scene.cover();check(!scene.visible(true,2.1)); // Loading/fade over an already-loaded village.
    scene.world(true,3);check(scene.visible(true,3.1) && !scene.visible(true,3.3));
    Notifications deferred;deferred.push({true,0,0,"Iron Sword"});
    scene.cover();if(scene.visible(true,3.1))deferred.advance(8);
    check(deferred.cards().empty() && deferred.queued()==1);
    scene.world(true,10);if(scene.visible(true,10))deferred.advance(0);
    check(deferred.cards().size()==1 && deferred.cards()[0].age==0);
    deferred.advance(4.99);check(deferred.cards().size()==1);
    deferred.advance(.02);check(deferred.cards().empty());
    FeedbackTimer feedback;check(!feedback.visible());
    feedback.show({true,"Iron Sword"});feedback.advance(2.99);check(feedback.visible() && feedback.value().pinned);
    feedback.advance(.02);check(!feedback.visible());
    feedback.show({false,"Iron Sword"});check(feedback.visible() && !feedback.value().pinned && feedback.age()==0);
    feedback.clear();check(!feedback.visible());
}
