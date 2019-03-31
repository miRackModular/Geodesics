//***********************************************************************************************
//Geodesics: A modular collection for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//  and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


Plugin *pluginInstance;


void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelBlackHoles);
	p->addModel(modelPulsars);
	// p->addModel(modelBranes);
	// p->addModel(modelIons);
	p->addModel(modelEntropia);
	p->addModel(modelBlankLogo);
	p->addModel(modelBlankInfo);
}




// other


bool Trigger::process(float in) {
	switch (state) {
		case LOW:
			if (in >= 1.0f) {
				state = HIGH;
				return true;
			}
			break;
		case HIGH:
			if (in <= 0.1f) {
				state = LOW;
			}
			break;
		default:
			if (in >= 1.0f) {
				state = HIGH;
			}
			else if (in <= 0.1f) {
				state = LOW;
			}
			break;
	}
	return false;
}	

int getWeighted1to8random() {
	int	prob = random::u32() % 1000;
	if (prob < 175)
		return 1;
	else if (prob < 330) // 175 + 155
		return 2;
	else if (prob < 475) // 175 + 155 + 145
		return 3;
	else if (prob < 610) // 175 + 155 + 145 + 135
		return 4;
	else if (prob < 725) // 175 + 155 + 145 + 135 + 115
		return 5;
	else if (prob < 830) // 175 + 155 + 145 + 135 + 115 + 105
		return 6;
	else if (prob < 925) // 175 + 155 + 145 + 135 + 115 + 105 + 95
		return 7;
	return 8;
}
