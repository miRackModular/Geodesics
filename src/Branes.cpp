//***********************************************************************************************
//Colliding Sample and Hold module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//Also based on code from Joel Robichaud's Nohmad Noise module
//See ./LICENSE.txt for all licenses
//
//***********************************************************************************************


#include <dsp/filter.hpp>
#include <random>
#include "Geodesics.hpp"


// By Joel Robichaud - Nohmad Noise module
struct NoiseGenerator {
	std::mt19937 rng;
	std::uniform_real_distribution<float> uniform;

	NoiseGenerator() : uniform(-1.0f, 1.0f) {
		rng.seed(std::random_device()());
	}

	float white() {
		return uniform(rng);
	}
};


//*****************************************************************************


// By Joel Robichaud - Nohmad Noise module
struct PinkFilter {
	float b0, b1, b2, b3, b4, b5, b6; // Coefficients
	float y; // Out

	void process(float x) {
		b0 = 0.99886f * b0 + x * 0.0555179f;
		b1 = 0.99332f * b1 + x * 0.0750759f;
		b2 = 0.96900f * b2 + x * 0.1538520f;
		b3 = 0.86650f * b3 + x * 0.3104856f;
		b4 = 0.55000f * b4 + x * 0.5329522f;
		b5 = -0.7616f * b5 - x * 0.0168980f;
		y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362f;
		b6 = x * 0.115926f;
	}

	float pink() {
		return y;
	}
};


//*****************************************************************************


struct Branes : Module {
	enum ParamIds {
		ENUMS(TRIG_BYPASS_PARAMS, 2),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 14),
		ENUMS(TRIG_INPUTS, 2),
		ENUMS(TRIG_BYPASS_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 14),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(BYPASS_CV_LIGHTS, 2 * 2),// room for white-red
		ENUMS(BYPASS_TRIG_LIGHTS, 2 * 2),// room for white-red
		NUM_LIGHTS
	};
	
	
	// Constants
	// S&H are numbered 0 to 6 in BraneA from lower left to lower right
	// S&H are numbered 7 to 13 in BraneB from top right to top left
	enum NoiseId {NONE, WHITE, PINK, RED, BLUE};//use negative value for inv phase
	int noiseSources[14] = {PINK, RED, BLUE, WHITE, -BLUE, -RED, -PINK,   -PINK, -RED, -BLUE, WHITE, BLUE, RED, PINK};

	// Need to save, with reset
	bool trigBypass[2];
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	float heldOuts[14];
	
	// No need to save, no reset
	SchmittTrigger sampleTriggers[2];
	SchmittTrigger trigBypassTriggers[2];
	float trigLights[2];
	
	NoiseGenerator whiteNoise;
	PinkFilter pinkFilter[2];
	RCFilter redFilter[2];
	RCFilter blueFilter[2];
	PinkFilter pinkForBlueFilter[2];

	bool cacheHitRed[2];// no need to init; index is braneIndex
	float cacheValRed[2];
	bool cacheHitBlue[2];// no need to init; index is braneIndex
	float cacheValBlue[2];
	bool cacheHitPink[2];// no need to init; index is braneIndex
	float cacheValPink[2];

	
	Branes() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		
		// No need to save, no reset		
		for (int i = 0; i < 2; i++) {
			sampleTriggers[i].reset();
			trigBypassTriggers[i].reset();
			trigLights[i] = 0.0f;
		}
		redFilter[0].setCutoff(441.0f / engineGetSampleRate());
		redFilter[1].setCutoff(441.0f / engineGetSampleRate());
		blueFilter[0].setCutoff(44100.0f / engineGetSampleRate());
		blueFilter[1].setCutoff(44100.0f / engineGetSampleRate());
		
		onReset();
	}

	
	// widgets are not yet created when module is created 
	// even if widgets not created yet, can use params[] and should handle 0.0f value since step may call 
	//   this before widget creation anyways
	// called from the main thread if by constructor, called by engine thread if right-click initialization
	//   when called by constructor, module is created before the first step() is called
	void onReset() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++)
			trigBypass[i] = false;
		
		// No need to save, with reset
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++)
			trigBypass[i] = (randomu32() % 2) > 0;
		
		// No need to save, with reset
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// trigBypass
		json_object_set_new(rootJ, "trigBypass0", json_real(trigBypass[0]));
		json_object_set_new(rootJ, "trigBypass1", json_real(trigBypass[1]));

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// trigBypass
		json_t *trigBypass0J = json_object_get(rootJ, "trigBypass0");
		if (trigBypass0J)
			trigBypass[0] = json_real_value(trigBypass0J);
		json_t *trigBypass1J = json_object_get(rootJ, "trigBypass1");
		if (trigBypass1J)
			trigBypass[1] = json_real_value(trigBypass1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// No need to save, with reset
		for (int i = 0; i < 14; i++)
			heldOuts[i] = 0.0f;
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		// trigBypass buttons and cv inputs
		for (int i = 0; i < 2; i++) {
			if (trigBypassTriggers[i].process(params[TRIG_BYPASS_PARAMS + i].value + inputs[TRIG_BYPASS_INPUTS + i].value)) {
				trigBypass[i] = !trigBypass[i];
			}
		}

		// trig inputs
		bool trigs[2];
		bool trigInputsActive[2];
		for (int i = 0; i < 2; i++)	{	
			trigs[i] = sampleTriggers[i].process(inputs[TRIG_INPUTS + i].value);
			if (trigs[i])
				trigLights[i] = 1.0f;
			trigInputsActive[i] = trigBypass[i] ? false : inputs[TRIG_INPUTS + i].active;
		}
		
		for (int i = 0; i < 2; i++) {
			cacheHitRed[i] = false;
			cacheHitBlue[i] = false;
			cacheHitPink[i] = false;
		}
		
		// sample and hold outputs (noise continually generated or else stepping non-white on S&H only will not work well because of filters)
		float noises[14];
		for (int sh = 0; sh < 14; sh++) {
			noises[sh] = getNoise(sh);
			if (outputs[OUT_OUTPUTS + sh].active) {
				int braneIndex = sh < 7 ? 0 : 1;
				if (trigInputsActive[braneIndex] || (sh == 13 && trigInputsActive[0]) || (sh == 6 && trigInputsActive[1])) {// if trigs connected (with crosstrigger mechanism)
					if ((trigInputsActive[braneIndex] && trigs[braneIndex]) || (sh == 13 && trigInputsActive[0] && trigs[0]) || (sh == 6 && trigInputsActive[1] && trigs[1])) {// if trig rising edge
						if (inputs[IN_INPUTS + sh].active)// if input cable
							heldOuts[sh] = inputs[IN_INPUTS + sh].value;// sample and hold input
						else
							heldOuts[sh] = noises[sh];//getNoise(sh);// sample and hold noise
					}
					// else no rising edge, so simply preserve heldOuts[sh], nothing to do
				}
				else { // no trig connected
					if (inputs[IN_INPUTS + sh].active)
						heldOuts[sh] = inputs[IN_INPUTS + sh].value;// copy of input if no trig and input
					else
						heldOuts[sh] = noises[sh];//getNoise(sh);// generate continuous noise if no trig and no input
				}
				outputs[OUT_OUTPUTS + sh].value = heldOuts[sh];
			}
		}
		
		// Lights
		for (int i = 0; i < 2; i++) {
			float red = trigBypass[i] ? 1.0f : 0.0f;
			float white = !trigBypass[i] ? trigLights[i] : 0.0f;
			lights[BYPASS_CV_LIGHTS + i * 2 + 0].value = white;
			lights[BYPASS_CV_LIGHTS + i * 2 + 1].value = red;
			lights[BYPASS_TRIG_LIGHTS + i * 2 + 0].value = white;
			lights[BYPASS_TRIG_LIGHTS + i * 2 + 1].value = red;
			trigLights[i] -= (trigLights[i] / lightLambda) * (float)engineGetSampleTime();
		}
		
	}// step()
	
	float getNoise(int sh) {
		// some of the code in here is from Joel Robichaud - Nohmad Noise module
		int noiseIndex = abs( noiseSources[sh] );
		if (noiseIndex == WHITE)
			return 5.0f * whiteNoise.white();
		
		int braneIndex = sh < 7 ? 0 : 1;
		if (noiseIndex == RED) {
			if (cacheHitRed[braneIndex])
				return -1.0 * cacheValRed[braneIndex];
			redFilter[braneIndex].process(whiteNoise.white());
			cacheValRed[braneIndex] = 5.0f * clamp(7.8f * redFilter[braneIndex].lowpass(), -1.0f, 1.0f);
			cacheHitRed[braneIndex] = true;
			return cacheValRed[braneIndex];
		}
		
		if (noiseIndex == PINK) {
			if (cacheHitPink[braneIndex])
				return -1.0 * cacheValPink[braneIndex];
			pinkFilter[braneIndex].process(whiteNoise.white());
			cacheValPink[braneIndex] = 5.0f * clamp(0.18f * pinkFilter[braneIndex].pink(), -1.0f, 1.0f);
			cacheHitPink[braneIndex] = true;
			return cacheValPink[braneIndex];
		}
			
		// noiseIndex == BLUE
		if (cacheHitBlue[braneIndex])
			return -1.0 * cacheValBlue[braneIndex];
		pinkForBlueFilter[braneIndex].process(whiteNoise.white());
		blueFilter[braneIndex].process(pinkForBlueFilter[braneIndex].pink());
		cacheValBlue[braneIndex] = 5.0f * clamp(0.64f * blueFilter[braneIndex].highpass(), -1.0f, 1.0f);
		cacheHitBlue[braneIndex] = true;
		return cacheValBlue[braneIndex];
	}
};


struct BranesWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Branes *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Branes *module = dynamic_cast<Branes*>(this->module);
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
		//menu->addChild(darkItem);

		return menu;
	}	
	
	BranesWidget(Branes *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BranesBG-01.svg")));
        //panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BranesBG-02.svg")));// no dark pannel for now
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		static constexpr float rowRulerHoldA = 119.5;
		static constexpr float rowRulerHoldB = 248.5f;
		static constexpr float radiusIn = 35.0f;
		static constexpr float radiusOut = 64.0f;
		static constexpr float offsetIn = 25.0f;
		static constexpr float offsetOut = 46.0f;
		
		
		// BraneA trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA), Port::INPUT, module, Branes::TRIG_INPUTS + 0, &module->panelTheme));
		
		// BraneA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldA), Port::INPUT, module, Branes::IN_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldA - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 2, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusIn), Port::INPUT, module, Branes::IN_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 4, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldA), Port::INPUT, module, Branes::IN_INPUTS + 5, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldA + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 6, &module->panelTheme));

		// BraneA outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldA), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldA - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldA - radiusOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 4, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldA), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 5, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldA + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 6, &module->panelTheme));
		
		
		// BraneB trig intput
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB), Port::INPUT, module, Branes::TRIG_INPUTS + 1, &module->panelTheme));
		
		// BraneB inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 7, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerHoldB), Port::INPUT, module, Branes::IN_INPUTS + 8, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetIn, rowRulerHoldB + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 9, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusIn), Port::INPUT, module, Branes::IN_INPUTS + 10, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB + offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 11, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerHoldB), Port::INPUT, module, Branes::IN_INPUTS + 12, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetIn, rowRulerHoldB - offsetIn), Port::INPUT, module, Branes::IN_INPUTS + 13, &module->panelTheme));


		// BraneB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 7, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusOut, rowRulerHoldB), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 8, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetOut, rowRulerHoldB + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 9, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerHoldB + radiusOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 10, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB + offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 11, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusOut, rowRulerHoldB), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 12, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetOut, rowRulerHoldB - offsetOut), Port::OUTPUT, module, Branes::OUT_OUTPUTS + 13, &module->panelTheme));
		
		
		static constexpr float rowRulerBypass = 345.5f;
		
		// Trigger bypass
		// buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 32.0f, rowRulerBypass), module, Branes::TRIG_BYPASS_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 32.0f, rowRulerBypass), module, Branes::TRIG_BYPASS_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// cv inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 65.0f, rowRulerBypass), Port::INPUT, module, Branes::TRIG_BYPASS_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 65.0f, rowRulerBypass), Port::INPUT, module, Branes::TRIG_BYPASS_INPUTS + 1, &module->panelTheme));
		// LEDs bottom
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter - 46.5f, rowRulerBypass), module, Branes::BYPASS_CV_LIGHTS + 0 * 2));
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter + 46.5f, rowRulerBypass), module, Branes::BYPASS_CV_LIGHTS + 1 * 2));
		// LEDs top
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter + 5.5f, rowRulerHoldA + 19.5f), module, Branes::BYPASS_TRIG_LIGHTS + 0 * 2));
		addChild(createLightCentered<SmallLight<GeoWhiteRedLight>>(Vec(colRulerCenter - 5.5f, rowRulerHoldB - 19.5f), module, Branes::BYPASS_TRIG_LIGHTS + 1 * 2));

	}
};

Model *modelBranes = Model::create<Branes, BranesWidget>("Geodesics", "Branes", "Branes", SAMPLE_AND_HOLD_TAG);

/*CHANGE LOG

0.6.0:
created

*/