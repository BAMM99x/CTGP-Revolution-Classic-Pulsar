#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {

namespace Settings {

u8 Params::radioCount[Params::pageCount] ={
    5, 5, 4, 3, 2, 4, //menu, race, host, OTT, KO, menu2
    2 //Extended Teams (user page 0)
};
u8 Params::scrollerCount[Params::pageCount] ={ 1, 1, 1, 0, 2, 2, 1 }; //menu, race, host, OTT, KO, menu2, Extended Teams

u8 Params::buttonsPerPagePerRow[Params::pageCount][Params::maxRadioCount] = //first row is PulsarSettingsType, 2nd is rowIdx of radio
{
    { 2, 2, 2, 3, 2, 0 }, //Menu 
    { 2, 2, 2, 2, 4, 0 }, //Race
    { 2, 4, 2, 3, 0, 0 }, //Host
    { 2, 2, 2, 0, 0, 0 }, //OTT
    { 2, 2, 0, 0, 0, 0 }, //KO
    { 2, 2, 2, 3, 0, 0 }, //Menu2
    { 2, 2, 0, 0, 0, 0 }, //Extended Teams: on/off (2), colore linea (2)
};

u8 Params::optionsPerPagePerScroller[Params::pageCount][Params::maxScrollerCount] =
{
    { 5, 7, 0, 0, 0}, //Menu 
    { 4, 0, 0, 0, 0}, //Race
    { 15, 0, 0, 0, 0}, //Host
    { 0, 0, 0, 0, 0}, //OTT
    { 4, 4, 0, 0, 0}, //KO
    { 16, 16, 0, 0, 0}, //Menu2
    { 4, 0, 0, 0, 0}, //Extended Teams: giocatori per squadra (2/3/4/6)
};

}//namespace Settings
}//namespace Pulsar