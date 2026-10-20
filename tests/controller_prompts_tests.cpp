#include "controller_prompts.hpp"
#include "input/bindings.hpp"
#include <cstdlib>
#include <iostream>
using namespace portablekit;
using namespace portablekit::fu::controller_prompts;
void check(bool value){if(!value){std::cerr<<"Controller prompts regression failed\n";std::exit(1);}}
int main(){
    check(label(Family::Nintendo,Action::ExtraLeft,"")=="ZL");
    check(label(Family::Nintendo,Action::ExtraRight,"")=="ZR");
    check(label(Family::PlayStation,Action::ExtraLeft,"")=="L2");
    check(label(Family::PlayStation,Action::ExtraRight,"")=="R2");
    check(label(Family::Xbox,Action::ExtraLeft,"")=="LT");
    check(label(Family::Generic,Action::ExtraRight,"")=="RT");
    check(label(Family::Keyboard,Action::ExtraLeft,"F6")=="F6");
    check(label(Family::Keyboard,Action::ExtraRight,"")=="Unbound");
    for(auto bindings:{input::default_bindings(),input::classic_bindings()}){
        auto result=input::read(bindings,[](auto b){return b==input::key(63);});
        check(result.extra_left && !result.extra_right && result.buttons==0);
        input::assign(bindings,input::Action::ExtraRight,input::key(5));
        const auto slots=bindings[static_cast<size_t>(input::Action::ExtraRight)];
        check(label(Family::Keyboard,Action::ExtraRight,input::format(slots)).find("B")!=std::string::npos);
        result=input::read(bindings,[](auto b){return b==input::key(5);});
        check(result.extra_right && !result.extra_left && result.buttons==0);
        result=input::read(bindings,[](auto){return false;});
        check(!result.extra_left && !result.extra_right);
    }
}
