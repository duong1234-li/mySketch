#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include <memory>

struct libinput;
struct libinput_device;
struct libinput_event;

class ofApp : public ofBaseApp{

	public:
		enum class Tool {
			Brush,
			Pen,
			Pencil,
			Marker,
			Paintbrush,
			Fill,
			MoveResize
		};

		enum class LayerItemType {
			Layer,
			Image
		};

		struct FillSpan {
			int x;
			int y;
			int width;
		};

		struct Stroke {
			std::vector<ofVec2f> points;
			std::vector<float> pressures;
			std::vector<FillSpan> fillSpans;
			ofColor color;
			float width;
			Tool tool;
			bool eraser;
		};

		struct Layer {
			std::string name;
			std::vector<Stroke> strokes;
			ofFbo image;
			bool visible = true;
			float opacity = 1.0f;
			ofImage layerImage;
			bool hasImage = false;
			ofVec2f imagePosition;
			float imageScale = 1.0f;
			ofVec2f transformPosition;
			float transformScale = 1.0f;
		};

		struct LayerPanelRow {
			LayerItemType type;
			std::size_t layerIndex;
		ofRectangle bounds;
		};

		void setup();
		void exit();
		void update();
		void draw();
		void resizeCanvas(int width, int height);
		void drawStroke(const Stroke &stroke);
		void drawStrokeLayer(const Stroke &stroke);
		void renderStroke(const Stroke &stroke);
		void applyStroke(Layer &layer, const Stroke &stroke);
		void rebuildLayer(Layer &layer);
		void rebuildCanvas();
				void detectFillPixelOrientation();
		Layer &activeLayer();
		void addLayer();
		void removeActiveLayer();
		void moveActiveLayer(int direction);
		void drawLayerPanel();
		void updateLayerPanelLayout();
		bool handleLayerPanelPress(int x, int y);
		void setActiveLayerOpacity(int x);
		void newProject();
		ofVec2f screenToCanvasPoint(int x, int y) const;
		ofVec2f canvasPointToLayer(const Layer &layer, const ofVec2f &point) const;
		ofRectangle strokeContentBounds(const Stroke &stroke) const;
		ofRectangle layerContentBounds(const Layer &layer) const;
		ofRectangle selectedContentBounds(const Layer &layer) const;
		void addImageToLayer(Layer &layer, const std::string &imagePath);
		void addImageToActiveLayer(const std::string &imagePath);
		void removeImageFromActiveLayer();
		void resetImageTransformForActiveLayer();
		void fillAt(int x, int y);
		void setupTabletPressureInput();
		void pollTabletPressure();
		void resetSimulatedPressure();
		void updateSimulatedPressure();
		float effectivePressure() const;
		void discoverTabletDevice();
		void drainTabletEvents();
		void applyTabletEvent(struct libinput_event *event);
		void endTabletContact();
		void handleTabletDeviceRemoved(struct libinput_device *device);
		static int openTabletDevice(const char *path, int flags, void *userData);
		static void closeTabletDevice(int fd, void *userData);
		void appendStrokePoint(const ofVec2f &point, float pressure);
		void undoLastStroke();
		void clearCanvas();
		void saveArtwork();
		void finishActiveStroke();
		void selectTool(Tool tool);
		void brushToolChanged(bool &enabled);
		void penToolChanged(bool &enabled);
		void pencilToolChanged(bool &enabled);
		void markerToolChanged(bool &enabled);
		void paintbrushToolChanged(bool &enabled);
		void fillToolChanged(bool &enabled);
		void moveResizeToolChanged(bool &enabled);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void mouseScrolled(int x, int y, float scrollX, float scrollY);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

	private:
		static constexpr float tabletContactThreshold = 0.02f;

		std::vector<std::unique_ptr<Layer>> layers;
		Stroke activeStroke;
		bool drawingStroke = false;
		bool pendingStroke = false;
		int pendingMotionSamples = 0;
		uint64_t pendingStrokeStartedAt = 0;
		bool draggingLayerOpacity = false;
		bool draggingLayerPanel = false;
		bool draggingLayerTransform = false;
		bool resizingLayerTransform = false;
		ofVec2f layerTransformDragStart;
		ofVec2f layerTransformPositionStart;
		ofVec2f layerPanelDragStart;
		ofVec2f layerPanelPositionStart;
		ofVec2f transformResizeStart;
		ofVec2f transformResizeAnchor;
		ofVec2f transformResizeLocalAnchor;
		ofVec2f transformResizeParentAnchor;
		float transformResizeStartScale = 1.0f;
		bool fillReadbackFlipped = false;
		struct libinput *libinputContext = nullptr;
		struct libinput_device *libinputDevice = nullptr;
		std::vector<std::string> libinputDevicePaths;
		std::string libinputDevicePath;
		uint64_t libinputRescanAt = 0;
		bool libinputPermissionDenied = false;
		bool tabletDeviceFound = false;
		bool tabletPressureSourceActive = false;
		bool tabletPressureHeld = false;
		float tabletPressure = 1.0f;
		bool simulatedPressureValid = false;
		float simulatedPressure = 1.0f;
		ofVec2f simulatedPressurePosition;
		uint64_t simulatedPressureUpdatedAt = 0;
		ofFbo canvas;
		ofFbo strokeLayer;
		ofFbo exportBuffer;
		ofRectangle canvasBounds;
		int canvasWidth = 1200;
		int canvasHeight = 800;
		float canvasViewScale = 1.0f;
		ofRectangle layerPanelBounds;
		ofRectangle layerPanelListBounds;
		ofRectangle layerNewProjectBounds;
		ofRectangle layerAddBounds;
		ofRectangle layerRemoveBounds;
		ofRectangle layerUpBounds;
		ofRectangle layerDownBounds;
		ofRectangle layerAddImageBounds;
		ofRectangle layerRemoveImageBounds;
		ofRectangle layerOpacityBounds;
		std::vector<LayerPanelRow> layerPanelRows;
		std::vector<ofRectangle> layerVisibilityBounds;
		float layerPanelScrollOffset = 0.0f;
		ofxPanel gui;
		ofxPanel layersPanel;
		ofParameter<ofColor> brushColor;
		ofParameter<bool> brushTool;
		ofParameter<bool> penTool;
		ofParameter<bool> pencilTool;
		ofParameter<bool> markerTool;
		ofParameter<bool> paintbrushTool;
		ofParameter<bool> fillTool;
		ofParameter<bool> moveResizeTool;
		ofParameter<float> brushSize;
		ofParameter<int> brushOpacity;
		ofParameter<float> pressureSensitivity;
		ofParameter<float> pressureSimulation;
		ofParameter<int> fillTolerance;
		ofParameter<bool> eraser;
		ofParameter<void> undoAction;
		ofParameter<void> clearAction;
		ofParameter<void> saveAction;
		ofColor backgroundColor;
		ofColor paperColor;
		Tool selectedTool = Tool::Brush;
		LayerItemType selectedLayerItemType = LayerItemType::Layer;
		std::size_t activeLayerIndex = 0;
		int nextLayerNumber = 0;
		
};
