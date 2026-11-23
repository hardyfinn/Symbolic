// Symbolic: an acoustic cymbal (and hi-hat) model.
// The sound engine is in SymbolicEngine.hpp, with notes on how it works.
#include "plugin.hpp"
#include "SymbolicEngine.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COL[6] = {12.0f, 35.6f, 59.2f, 82.8f, 106.4f, 130.0f};
static const float ROW_PLATE = 30.0f, ROW_STRIKE = 51.0f, ROW_HAT = 72.0f, ROW_CV = 94.5f, ROW_IO = 113.5f;

struct Symbolic : Module {
	enum ParamIds {
		SIZE_PARAM,
		WEIGHT_PARAM,
		BELL_PARAM,
		WOBBLE_PARAM,
		TRASH_PARAM,
		CAST_PARAM,
		POSITION_PARAM,
		STICK_PARAM,
		DECAY_PARAM,
		DAMP_PARAM,
		SHIMMER_PARAM,
		BUILD_PARAM,
		TUNE_PARAM,
		PEDAL_PARAM,
		CHICK_PARAM,
		WIDTH_PARAM,
		LEVEL_PARAM,
		HIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT,
		SIZE_INPUT,
		POSITION_INPUT,
		STICK_INPUT,
		DECAY_INPUT,
		SHIMMER_INPUT,
		TRIG_INPUT,
		VEL_INPUT,
		CHOKE_INPUT,
		PEDAL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		HIT_LIGHT,
		NUM_LIGHTS
	};

	symbolic::Engine engine;
	symbolic::Controls ctl;
	dsp::SchmittTrigger trigIn, chokeIn, hitButton;
	dsp::ClockDivider lightDivider;

	struct CastQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			return string::f("Cymbal no. %d", (int) std::round(getValue()));
		}
	};
	struct PositionQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			const char* w = v < 0.2f ? "bell" : v < 0.35f ? "bell / bow" : v < 0.75f ? "bow" : v < 0.88f ? "bow / edge" : "edge";
			return string::f("%.0f %% (%s)", v * 100.f, w);
		}
	};

	Symbolic() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SIZE_PARAM, 8.f, 24.f, 18.f, "Size (diameter)", "\"");
		configParam(WEIGHT_PARAM, 0.f, 1.f, 0.5f, "Weight (thin <-> heavy)", "%", 0.f, 100.f);
		configParam(BELL_PARAM, 0.f, 1.f, 0.5f, "Bell (size of the dome)", "%", 0.f, 100.f);
		configParam(WOBBLE_PARAM, 0.f, 1.f, 0.4f, "Wobble (slow beating of the low tones)", "%", 0.f, 100.f);
		configParam(TRASH_PARAM, 0.f, 1.f, 0.f, "Trash (china: pitch bend, fast beating, grit)", "%", 0.f, 100.f);
		configParam<CastQuantity>(CAST_PARAM, 1.f, 16.f, 1.f, "Cast (which individual cymbal)");
		getParamQuantity(CAST_PARAM)->snapEnabled = true;
		configParam<PositionQuantity>(POSITION_PARAM, 0.f, 1.f, 0.55f, "Position (bell <-> bow <-> edge)");
		configParam(STICK_PARAM, 0.f, 1.f, 0.1f, "Stick (hard tip <-> soft mallet)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.55f, "Decay", "%", 0.f, 100.f);
		configParam(DAMP_PARAM, 0.f, 1.f, 0.35f, "Damp (bright ring <-> dark)", "%", 0.f, 100.f);
		configParam(SHIMMER_PARAM, 0.f, 1.f, 0.5f, "Shimmer (wash after hard hits)", "%", 0.f, 100.f);
		configParam(BUILD_PARAM, 0.f, 1.f, 0.35f, "Build (how slowly the wash blooms)", "%", 0.f, 100.f);
		configParam(TUNE_PARAM, -24.f, 24.f, 0.f, "Tune", " semitones");
		configParam(PEDAL_PARAM, 0.f, 1.f, 0.f, "Hi-hat pedal (open <-> closed)", "%", 0.f, 100.f);
		configParam(CHICK_PARAM, 0.f, 1.f, 0.5f, "Foot chick level (when the pedal closes)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.7f, "Stereo width", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configButton(HIT_PARAM, "Hit");

		configInput(VOCT_INPUT, "V/OCT (tune)");
		configInput(SIZE_INPUT, "Size CV (+-5 V = full range)");
		configInput(POSITION_INPUT, "Position CV (+-5 V = full range, read at each hit)");
		configInput(STICK_INPUT, "Stick CV (+-5 V = full range, read at each hit)");
		configInput(DECAY_INPUT, "Decay CV (+-5 V = full range)");
		configInput(SHIMMER_INPUT, "Shimmer CV (+-5 V = full range)");
		configInput(TRIG_INPUT, "Trigger");
		configInput(VEL_INPUT, "Velocity (0-10 V, read at each trigger; unpatched = 7.5 V)");
		configInput(CHOKE_INPUT, "Choke (a trigger grabs the cymbal for 150 ms; a held gate keeps it held)");
		configInput(PEDAL_INPUT, "Hi-hat pedal CV (0-10 V, added to the PEDAL knob)");
		configOutput(LEFT_OUTPUT, "Left (mono when RIGHT is unpatched)");
		configOutput(RIGHT_OUTPUT, "Right");

		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	static float cv5(Input& in) { return in.getVoltage() * 0.1f; }

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);

		ctl.size = clamp(params[SIZE_PARAM].getValue() + inputs[SIZE_INPUT].getVoltage() * 1.6f, 8.f, 24.f);
		ctl.weight = params[WEIGHT_PARAM].getValue();
		ctl.bell = params[BELL_PARAM].getValue();
		ctl.wobble = params[WOBBLE_PARAM].getValue();
		ctl.trash = params[TRASH_PARAM].getValue();
		ctl.cast = clamp((int) std::round(params[CAST_PARAM].getValue()) - 1, 0, 15);
		ctl.position = clamp(params[POSITION_PARAM].getValue() + cv5(inputs[POSITION_INPUT]), 0.f, 1.f);
		ctl.stick = clamp(params[STICK_PARAM].getValue() + cv5(inputs[STICK_INPUT]), 0.f, 1.f);
		ctl.decay = clamp(params[DECAY_PARAM].getValue() + cv5(inputs[DECAY_INPUT]), 0.f, 1.f);
		ctl.damp = params[DAMP_PARAM].getValue();
		ctl.shimmer = clamp(params[SHIMMER_PARAM].getValue() + cv5(inputs[SHIMMER_INPUT]), 0.f, 1.f);
		ctl.build = params[BUILD_PARAM].getValue();
		ctl.tune = params[TUNE_PARAM].getValue() / 12.f + inputs[VOCT_INPUT].getVoltage();
		ctl.pedal = clamp(params[PEDAL_PARAM].getValue() + inputs[PEDAL_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		ctl.chick = params[CHICK_PARAM].getValue();
		ctl.width = params[WIDTH_PARAM].getValue();
		ctl.level = params[LEVEL_PARAM].getValue();
		ctl.choke = inputs[CHOKE_INPUT].getVoltage() > 1.f;

		bool hit = hitButton.process(params[HIT_PARAM].getValue());
		if (trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f) || hit) {
			float vel = 0.75f;
			if (inputs[VEL_INPUT].isConnected() && !hit)
				vel = clamp(inputs[VEL_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
			engine.trigger(vel, ctl);
		}
		if (chokeIn.process(inputs[CHOKE_INPUT].getVoltage(), 0.1f, 1.f))
			engine.chokeNow();

		engine.process(ctl);
		if (outputs[RIGHT_OUTPUT].isConnected()) {
			outputs[LEFT_OUTPUT].setVoltage(engine.outL);
			outputs[RIGHT_OUTPUT].setVoltage(engine.outR);
		} else {
			outputs[LEFT_OUTPUT].setVoltage(0.5f * (engine.outL + engine.outR));
		}

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(engine.meter, dt, 30.f);
		}
	}

	// right-click starting points
	void setAll(std::initializer_list<std::pair<int, float>> v) {
		for (auto& p : v) params[p.first].setValue(p.second);
	}
};

struct SymbolicWidget : ModuleWidget {
	SymbolicWidget(Symbolic* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Symbolic.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const int plate[6] = {Symbolic::SIZE_PARAM, Symbolic::WEIGHT_PARAM, Symbolic::BELL_PARAM,
		                      Symbolic::WOBBLE_PARAM, Symbolic::TRASH_PARAM, Symbolic::CAST_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_PLATE)), module, plate[i]));

		const int strike[6] = {Symbolic::POSITION_PARAM, Symbolic::STICK_PARAM, Symbolic::DECAY_PARAM,
		                       Symbolic::DAMP_PARAM, Symbolic::SHIMMER_PARAM, Symbolic::BUILD_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_STRIKE)), module, strike[i]));

		const int hat[5] = {Symbolic::TUNE_PARAM, Symbolic::PEDAL_PARAM, Symbolic::CHICK_PARAM,
		                    Symbolic::WIDTH_PARAM, Symbolic::LEVEL_PARAM};
		for (int i = 0; i < 5; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_HAT)), module, hat[i]));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(COL[5], ROW_HAT)), module, Symbolic::HIT_PARAM, Symbolic::HIT_LIGHT));

		const int cvIns[6] = {Symbolic::VOCT_INPUT, Symbolic::SIZE_INPUT, Symbolic::POSITION_INPUT,
		                      Symbolic::STICK_INPUT, Symbolic::DECAY_INPUT, Symbolic::SHIMMER_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_CV)), module, cvIns[i]));

		const int ioIns[4] = {Symbolic::TRIG_INPUT, Symbolic::VEL_INPUT, Symbolic::CHOKE_INPUT, Symbolic::PEDAL_INPUT};
		for (int i = 0; i < 4; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_IO)), module, ioIns[i]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[4], ROW_IO)), module, Symbolic::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_IO)), module, Symbolic::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Symbolic* m = dynamic_cast<Symbolic*>(this->module);
		if (!m)
			return;
		typedef Symbolic S;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Starting points"));
		menu->addChild(createMenuItem("Ride, 20\"", "", [=]() {
			m->setAll({{S::SIZE_PARAM, 20.f}, {S::WEIGHT_PARAM, 0.7f}, {S::BELL_PARAM, 0.6f}, {S::WOBBLE_PARAM, 0.45f},
			           {S::TRASH_PARAM, 0.f}, {S::POSITION_PARAM, 0.5f}, {S::STICK_PARAM, 0.08f}, {S::DECAY_PARAM, 0.6f},
			           {S::DAMP_PARAM, 0.3f}, {S::SHIMMER_PARAM, 0.3f}, {S::BUILD_PARAM, 0.3f}, {S::PEDAL_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Crash, 16\"", "", [=]() {
			m->setAll({{S::SIZE_PARAM, 16.f}, {S::WEIGHT_PARAM, 0.35f}, {S::BELL_PARAM, 0.4f}, {S::WOBBLE_PARAM, 0.4f},
			           {S::TRASH_PARAM, 0.f}, {S::POSITION_PARAM, 0.95f}, {S::STICK_PARAM, 0.2f}, {S::DECAY_PARAM, 0.5f},
			           {S::DAMP_PARAM, 0.35f}, {S::SHIMMER_PARAM, 0.75f}, {S::BUILD_PARAM, 0.45f}, {S::PEDAL_PARAM, 0.f}});
		}));
		menu->addChild(createMenuItem("Hi-hat, 14\" (play it with PEDAL)", "", [=]() {
			m->setAll({{S::SIZE_PARAM, 14.f}, {S::WEIGHT_PARAM, 0.6f}, {S::BELL_PARAM, 0.3f}, {S::WOBBLE_PARAM, 0.3f},
			           {S::TRASH_PARAM, 0.f}, {S::POSITION_PARAM, 0.75f}, {S::STICK_PARAM, 0.08f}, {S::DECAY_PARAM, 0.45f},
			           {S::DAMP_PARAM, 0.3f}, {S::SHIMMER_PARAM, 0.4f}, {S::BUILD_PARAM, 0.1f}, {S::PEDAL_PARAM, 1.f}});
		}));
	}
};

Model* modelSymbolic = createModel<Symbolic, SymbolicWidget>("Symbolic");
