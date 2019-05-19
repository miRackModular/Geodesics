//***********************************************************************************************
//Bi-dimensional multimixer module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct chanVol {// a mixMap for an output has four of these, for each quadrant that can map to its output
	float vol;// 0.0 to 1.0
	float chan;// channel input number (0 to 15), garbage when vol is 0.0
};


struct mixMapOutput {
	chanVol cvs[4];// an output can have a mix of at most 4 inputs

	void init() {
		for (int i = 0; i < 4; i++) {
			cvs[i].vol = 0.0f;// when this is non-zero, chan is assuredly valid
		}	
	}
	
	void insert(float _vol, float _chan) {// _vol is never 0.0f
		assert(_vol != 0.0f);
		for (int i = 0; i < 4; i++) {
			if (cvs[i].vol == 0.0f) {
				cvs[i].vol = _vol;
				cvs[i].chan = _chan;
				return;
			}
		}
		assert(false);
	}
};
	

//*****************************************************************************


struct Torus : Module {
	enum ParamIds {
		GAIN_PARAM,
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(MIX_INPUTS, 16),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(MIX_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		DECAY_LIGHT,
		CONSTANT_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	// none
	
	// Need to save
	int panelTheme;
	int mixmode;// 0 is decay, 1 is constant
	
	// No need to save
	unsigned int lightRefreshCounter = 0;
	Trigger modeTrigger;
	mixMapOutput mixMap[7];// 7 outputs
	
	
	Torus() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(Torus::MODE_PARAM, 0.0f, 1.0f, 0.0f, "Mode");
		configParam(Torus::GAIN_PARAM, 0.0f, 2.0f, 1.0f, "Gain");

		onReset();

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		mixmode = 0;
		updateMixMap();
	}

	
	void onRandomize() override {
	}
	

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// mixmode
		json_object_set_new(rootJ, "mixmode", json_integer(mixmode));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// mixmode
		json_t *mixmodeJ = json_object_get(rootJ, "mixmode");
		if (mixmodeJ)
			mixmode = json_integer_value(mixmodeJ);
	}

	void process(const ProcessArgs &args) override {		
		// user inputs
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
			// mixmode
			if (modeTrigger.process(params[MODE_PARAM].getValue())) {
				if (++mixmode > 1)
					mixmode = 0;
			}
			
			updateMixMap();
		}// userInputs refresh
		
		
		// mixer code
		for (int outi = 0; outi < 7; outi++) {
			outputs[MIX_OUTPUTS + outi].setVoltage(clamp(calcOutput(outi) * params[GAIN_PARAM].getValue(), -10.0f, 10.0f));
		}
		

		// lights
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			lights[DECAY_LIGHT].setBrightness(mixmode == 0 ? 1.0f : 0.0f);
			lights[CONSTANT_LIGHT].setBrightness(mixmode == 1 ? 1.0f : 0.0f);
		}// lightRefreshCounter
	}// step()
	
	void updateMixMap() {
		for (int outi = 0; outi < 7; outi++) {
			mixMap[outi].init();
		}
		
		// scan inputs for upwards flow
		int distanceUL = 1;
		int distanceUR = 1;
		for (int ini = 1; ini < 8; ini++) {
			distanceUL++;
			distanceUR++;
			
			// left side
			if (inputs[MIX_INPUTS + ini].isConnected()) {
				for (int outi = ini - 1 ; outi >= 0; outi--) {
					int numerator = (distanceUL - ini + outi);
					if (numerator == 0) 
						break;
					float vol = (mixmode == 0 ? ((float)numerator) / ((float)(distanceUL)) : 1.0f);
					mixMap[outi].insert(vol, ini);
				}
				distanceUL = 1;
			}
			
			// right side
			if (inputs[MIX_INPUTS + 8 + ini].isConnected()) {
				for (int outi = ini - 1 ; outi >= 0; outi--) {
					int numerator = (distanceUR - ini + outi);
					if (numerator == 0) 
						break;
					float vol = (mixmode == 0 ? ((float)numerator) / ((float)(distanceUR)) : 1.0f);
					mixMap[outi].insert(vol, 8 + ini);
				}
				distanceUR = 1;
			}			
		}	

		// scan inputs for downward flow
		int distanceDL = 1;
		int distanceDR = 1;
		for (int ini = 6; ini >= 0; ini--) {
			distanceDL++;
			distanceDR++;
			
			// left side
			if (inputs[MIX_INPUTS + ini].isConnected()) {
				for (int outi = ini ; outi < 7; outi++) {
					int numerator = (distanceDL - 1 + ini - outi);
					if (numerator == 0) 
						break;
					float vol = (mixmode == 0 ? ((float)numerator) / ((float)(distanceDL)) : 1.0f);
					mixMap[outi].insert(vol, ini);
				}
				distanceDL = 1;
			}
			
			// right side
			if (inputs[MIX_INPUTS + 8 + ini].isConnected()) {
				for (int outi = ini ; outi < 7; outi++) {
					int numerator = (distanceDR - 1 + ini - outi);
					if (numerator == 0) 
						break;
					float vol = (mixmode == 0 ? ((float)numerator) / ((float)(distanceDR)) : 1.0f);
					mixMap[outi].insert(vol, 8 + ini);
				}
				distanceDR = 1;
			}		
		}	
	}
	
	inline float calcOutput(int outi) {
		float outputValue = 0.0f;
		for (int i = 0; i < 4; i++) {
			 float vol = mixMap[outi].cvs[i].vol;
			 if (vol > 0.0f) {
				outputValue += inputs[MIX_INPUTS + mixMap[outi].cvs[i].chan].getVoltage() * vol;
			 }
		}
		return outputValue;
	}
};


struct TorusWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Torus *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Torus *module = dynamic_cast<Torus*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// Geodesics.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// Geodesics.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
	}	
	
	TorusWidget(Torus *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Torus-WL.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Torus-WL.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;

		// mixmode button
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter, 380.0f - 329.5f), module, Torus::MODE_PARAM, module ? &module->panelTheme : NULL));
		
		// decay and constant lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 12.5f, 380.0f - 322.5f), module, Torus::DECAY_LIGHT));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 12.5f, 380.0f - 322.5f), module, Torus::CONSTANT_LIGHT));

		// gain knob
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, 380 - 294), module, Torus::GAIN_PARAM, module ? &module->panelTheme : NULL));
		
		// inputs
		static const int offsetY = 34;
		for (int i = 0; i < 8; i++) {
			addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 22.5f, 380 - (270 - offsetY * i)), true, module, Torus::MIX_INPUTS + i, module ? &module->panelTheme : NULL));
			addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 22.5f, 380 - (270 - offsetY * i)), true, module, Torus::MIX_INPUTS + 8 + i, module ? &module->panelTheme : NULL));
		}
		
		// mix outputs
		for (int i = 0; i < 7; i++) {
			addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380 - (253 - offsetY * i)), false, module, Torus::MIX_OUTPUTS + i, module ? &module->panelTheme : NULL));
		}
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Torus*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Torus*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelTorus = createModel<Torus, TorusWidget>("Torus");

/*CHANGE LOG

*/