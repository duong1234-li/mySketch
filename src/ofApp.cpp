#include "ofApp.h"
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
#include <X11/extensions/XInput2.h>
#include <cctype>
#endif

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofSetBackgroundAuto(true);
	ofEnableAlphaBlending();
	ofSetLineWidth(1.2f);

	backgroundColor = ofColor(8, 12, 18);
	paperColor = ofColor(248, 246, 238);
	resizeCanvas(ofGetWidth(), ofGetHeight());

	gui.setup("CANVAS TOOLS");
	gui.setPosition(16, 16);
	gui.setHeaderBackgroundColor(ofColor(28, 130, 142));
	gui.setBackgroundColor(ofColor(8, 15, 20, 230));
	gui.setBorderColor(ofColor(55, 100, 108));
	gui.setTextColor(ofColor(220, 235, 232));
	gui.setFillColor(ofColor(39, 164, 157));
	gui.add(brushColor.set("Brush color", ofColor(228, 83, 71), ofColor(0), ofColor(255)));
	gui.add(brushTool.set("Brush", true));
	gui.add(penTool.set("Pen", false));
	gui.add(pencilTool.set("Pencil", false));
	gui.add(markerTool.set("Marker", false));
	gui.add(paintbrushTool.set("Paintbrush", false));
	gui.add(fillTool.set("Fill bucket", false));
	gui.add(brushSize.set("Brush size", 12.0f, 1.0f, 80.0f));
	gui.add(brushOpacity.set("Opacity", 255, 10, 255));
	gui.add(fillTolerance.set("Fill tolerance", 0, 0, 32));
	gui.add(eraser.set("Eraser", false));
	gui.add(undoAction.set("Undo stroke"));
	gui.add(clearAction.set("Clear active layer"));
	gui.add(saveAction.set("Export PNG"));
	undoAction.addListener(this, &ofApp::undoLastStroke);
	clearAction.addListener(this, &ofApp::clearCanvas);
	saveAction.addListener(this, &ofApp::saveArtwork);
	brushTool.addListener(this, &ofApp::brushToolChanged);
	penTool.addListener(this, &ofApp::penToolChanged);
	pencilTool.addListener(this, &ofApp::pencilToolChanged);
	markerTool.addListener(this, &ofApp::markerToolChanged);
	paintbrushTool.addListener(this, &ofApp::paintbrushToolChanged);
	fillTool.addListener(this, &ofApp::fillToolChanged);
	addLayer();
	setupTabletPressureInput();
}

//--------------------------------------------------------------
void ofApp::exit(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (tabletDisplay) {
		XCloseDisplay(static_cast<Display *>(tabletDisplay));
		tabletDisplay = nullptr;
	}
#endif
}

//--------------------------------------------------------------
void ofApp::update(){
	pollTabletPressure();
}

//--------------------------------------------------------------
void ofApp::setupTabletPressureInput(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	Display *display = XOpenDisplay(nullptr);
	if (!display) {
		ofLogWarning("ofApp") << "Could not open X display for tablet pressure input";
		return;
	}

	int eventBase = 0;
	int errorBase = 0;
	if (!XQueryExtension(display, "XInputExtension", &tabletEventOpcode, &eventBase, &errorBase)) {
		ofLogWarning("ofApp") << "XInput2 is unavailable; tablet pressure is disabled";
		XCloseDisplay(display);
		return;
	}

	int major = 2;
	int minor = 0;
	if (XIQueryVersion(display, &major, &minor) != Success) {
		ofLogWarning("ofApp") << "XInput2 version 2 is unavailable; tablet pressure is disabled";
		XCloseDisplay(display);
		return;
	}

	int deviceCount = 0;
	XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &deviceCount);
	for (int i = 0; i < deviceCount; ++i) {
		if (devices[i].use != XISlavePointer && devices[i].use != XIFloatingSlave) {
			continue;
		}
		std::string deviceName = devices[i].name ? devices[i].name : "";
		std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(),
			[](unsigned char value){ return static_cast<char>(std::tolower(value)); });
		if (deviceName.find("eraser") != std::string::npos ||
			deviceName.find("cursor") != std::string::npos) {
			auxiliaryTabletDeviceIds.push_back(devices[i].deviceid);
			continue;
		}
		for (int j = 0; j < devices[i].num_classes; ++j) {
			if (devices[i].classes[j]->type != XIValuatorClass) {
				continue;
			}
			auto *valuator = reinterpret_cast<XIValuatorClassInfo *>(devices[i].classes[j]);
			if (valuator->label == None) {
				continue;
			}
			char *axisName = XGetAtomName(display, valuator->label);
			std::string normalizedName = axisName ? axisName : "";
			if (axisName) {
				XFree(axisName);
			}
			std::transform(normalizedName.begin(), normalizedName.end(), normalizedName.begin(),
				[](unsigned char value){ return static_cast<char>(std::tolower(value)); });
			if (normalizedName.find("pressure") != std::string::npos && valuator->max > valuator->min) {
				tabletPressureAxes.push_back({devices[i].deviceid, valuator->number,
					valuator->min, valuator->max});
				ofLogNotice("ofApp") << "Tablet pressure axis detected on " << devices[i].name;
			}
		}
	}
	XIFreeDeviceInfo(devices);
	if (tabletPressureAxes.empty()) {
		ofLogNotice("ofApp") << "No pressure-capable XInput2 device detected; mouse strokes use full pressure";
		XCloseDisplay(display);
		tabletEventOpcode = -1;
		return;
	}

	// Do not select XI events on the main window. In XWayland, the stylus can
	// otherwise steal pen button events before ofxGui receives them, so the color
	// picker and other UI controls stop responding to the pen.
	XCloseDisplay(display);
	tabletDisplay = nullptr;
	tabletEventOpcode = -1;
	ofLogNotice("ofApp") << "XI event subscription disabled; pen input is left to GLFW/ofxGui";
#endif
}

//--------------------------------------------------------------
void ofApp::pollTabletPressure(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (!tabletDisplay || tabletEventOpcode < 0) {
		return;
	}
	Display *display = static_cast<Display *>(tabletDisplay);
	while (XPending(display) > 0) {
		XEvent event;
		XNextEvent(display, &event);
		if (event.type != GenericEvent || event.xcookie.extension != tabletEventOpcode ||
			!XGetEventData(display, &event.xcookie)) {
			continue;
		}

		if (event.xcookie.evtype == XI_Leave) {
			auto *leave = static_cast<XILeaveEvent *>(event.xcookie.data);
			if (leave->sourceid == tabletPressureDeviceId) {
				if (tabletStrokeInput && pendingStroke) {
					finishActiveStroke();
				}
				tabletPressure = 1.0f;
				tabletPressureSourceActive = false;
				tabletPressureDeviceId = -1;
			}
		} else if (event.xcookie.evtype == XI_Motion) {
			auto *motion = static_cast<XIDeviceEvent *>(event.xcookie.data);
			const TabletPressureAxis *pressureAxis = nullptr;
			for (const auto &axis : tabletPressureAxes) {
				if (axis.deviceId == motion->sourceid) {
					pressureAxis = &axis;
					break;
				}
			}
			if (pressureAxis) {
				const float previousPressure = tabletPressure;
				int valueIndex = 0;
				bool pressureUpdated = false;
				for (int axis = 0; axis < motion->valuators.mask_len * 8; ++axis) {
					if (!XIMaskIsSet(motion->valuators.mask, axis)) {
						continue;
					}
					const double value = motion->valuators.values[valueIndex++];
					if (axis == pressureAxis->axis) {
						tabletPressure = ofClamp((value - pressureAxis->minimum) /
							(pressureAxis->maximum - pressureAxis->minimum), 0.0, 1.0);
						pressureUpdated = true;
						break;
					}
				}
				if (pressureUpdated) {
					tabletPressureSourceActive = true;
					tabletPressureDeviceId = motion->sourceid;
				}
				if (tabletPressureSourceActive && tabletPressureDeviceId == motion->sourceid) {
					tabletPointerPosition.set(motion->event_x, motion->event_y);
					tabletPointerPositionValid = true;
					const ofVec2f localPoint(tabletPointerPosition.x - canvasBounds.x,
						tabletPointerPosition.y - canvasBounds.y);
					if (tabletPressure > 0.02f && pendingStroke) {
						if (!tabletStrokeInput) {
							tabletStrokeInput = true;
							drawingStroke = false;
							pendingMotionSamples = 0;
							activeStroke.points.clear();
							activeStroke.pressures.clear();
							activeStroke.points.push_back(localPoint);
							activeStroke.pressures.push_back(tabletPressure);
							tabletStrokePositionInitialized = true;
						} else if (!tabletStrokePositionInitialized) {
							activeStroke.points.front() = localPoint;
							activeStroke.pressures.front() = tabletPressure;
							tabletStrokePositionInitialized = true;
						}
						appendStrokePoint(localPoint, tabletPressure);
					} else if (pressureUpdated && previousPressure > 0.02f &&
						tabletPressure <= 0.02f && tabletStrokeInput && pendingStroke) {
						if (drawingStroke) {
							finishActiveStroke();
						} else {
							pendingStroke = false;
							activeStroke.points.clear();
							activeStroke.pressures.clear();
							tabletStrokeInput = false;
							tabletStrokePositionInitialized = false;
						}
					}
				}
			} else {
				const bool auxiliaryTabletDevice = std::find(auxiliaryTabletDeviceIds.begin(),
					auxiliaryTabletDeviceIds.end(), motion->sourceid) != auxiliaryTabletDeviceIds.end();
				if (!auxiliaryTabletDevice && tabletPressureSourceActive &&
					motion->sourceid != tabletPressureDeviceId) {
					tabletPressure = 1.0f;
					tabletPressureSourceActive = false;
					tabletPressureDeviceId = -1;
				}
			}
		}
		XFreeEventData(display, &event.xcookie);
	}
#endif
}

//--------------------------------------------------------------
void ofApp::appendStrokePoint(const ofVec2f &point, float pressure){
	if (!pendingStroke) {
		return;
	}
	const float segmentLength = (point - activeStroke.points.back()).length();
	if (segmentLength <= 0.5f) {
		return;
	}
	activeStroke.points.emplace_back(point);
	activeStroke.pressures.push_back(pressure);
	if (drawingStroke) {
		return;
	}
	++pendingMotionSamples;
	const uint64_t heldFor = ofGetElapsedTimeMillis() - pendingStrokeStartedAt;
	const float distanceFromPress = (point - activeStroke.points.front()).length();
	if (pendingMotionSamples >= 3 && distanceFromPress >= 12.0f && heldFor >= 50) {
		drawingStroke = true;
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofBackground(backgroundColor);
	ofSetColor(0, 0, 0, 70);
	ofDrawRectangle(canvasBounds.x + 5, canvasBounds.y + 5,
		canvasBounds.width, canvasBounds.height);
	ofSetColor(255);
	canvas.draw(canvasBounds.x, canvasBounds.y);

	if (drawingStroke) {
		ofPushMatrix();
		ofTranslate(canvasBounds.x, canvasBounds.y);
		drawStroke(activeStroke);
		ofPopMatrix();
	}

	gui.draw();
	drawLayerPanel();
	ofSetColor(220, 230, 232);
	ofDrawBitmapString("1 BRUSH  2 PEN  3 PENCIL  4 MARKER  5 PAINTBRUSH  6 FILL  E ERASER", 16, ofGetHeight() - 16);
}

//--------------------------------------------------------------
void ofApp::resizeCanvas(int width, int height){
	const float oldWidth = canvas.isAllocated() ? canvas.getWidth() : 0.0f;
	const float oldHeight = canvas.isAllocated() ? canvas.getHeight() : 0.0f;
	canvasBounds.set(286, 52, std::max(1, width - 310), std::max(1, height - 76));

	const float scaleX = oldWidth > 0 ? canvasBounds.width / oldWidth : 1.0f;
	const float scaleY = oldHeight > 0 ? canvasBounds.height / oldHeight : 1.0f;
	if (oldWidth > 0 && oldHeight > 0) {
		for (auto &layer : layers) {
			for (auto &stroke : layer->strokes) {
				for (auto &point : stroke.points) {
					point.x *= scaleX;
					point.y *= scaleY;
				}
				for (auto &span : stroke.fillSpans) {
					span.x = static_cast<int>(span.x * scaleX);
					span.y = static_cast<int>(span.y * scaleY);
					span.width = static_cast<int>(span.width * scaleX);
				}
				stroke.width *= (scaleX + scaleY) * 0.5f;
			}
		}
	}

	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	canvas.allocate(settings);
	strokeLayer.allocate(settings);
	exportBuffer.allocate(settings);
	detectFillPixelOrientation();
	for (auto &layer : layers) {
		layer->image.allocate(settings);
		rebuildLayer(*layer);
	}
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::detectFillPixelOrientation(){
	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofSetColor(255, 0, 0, 255);
	ofDrawRectangle(8, 0, 12, 12);
	ofPopStyle();
	strokeLayer.end();

	ofPixels pixels;
	strokeLayer.readToPixels(pixels);
	const int pixelHeight = static_cast<int>(pixels.getHeight());
	int markerRow = -1;
	for (int y = 0; y < pixelHeight && markerRow < 0; ++y) {
		for (int x = 8; x < 20; ++x) {
			const ofColor pixel = pixels.getColor(x, y);
			if (pixel.r > 200 && pixel.g < 40) {
				markerRow = y;
				break;
			}
		}
	}
	fillReadbackFlipped = markerRow > pixelHeight / 2;

	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	strokeLayer.end();
}

//--------------------------------------------------------------

void ofApp::drawStroke(const Stroke &stroke){
	const ofColor color = stroke.eraser ? ofColor(255) : stroke.color;
	ofPushStyle();
	ofSetColor(color);
	if (stroke.tool == Tool::Fill) {
		for (const auto &span : stroke.fillSpans) {
			ofDrawRectangle(span.x, span.y, span.width, 1);
		}
		ofPopStyle();
		return;
	}
	auto pressureAt = [&stroke](std::size_t index){
		const float pressure = index < stroke.pressures.size() ? stroke.pressures[index] : 1.0f;
		return std::sqrt(ofClamp(pressure, 0.0f, 1.0f));
	};
	if (stroke.points.size() == 1) {
		ofDrawCircle(stroke.points.front(), stroke.width * pressureAt(0) * 0.5f);
		ofPopStyle();
		return;
	}

	if (stroke.tool == Tool::Paintbrush && !stroke.eraser) {
		for (std::size_t i = 1; i < stroke.points.size(); ++i) {
			const ofVec2f start = stroke.points[i - 1];
			const ofVec2f end = stroke.points[i];
			const float pressure = (pressureAt(i - 1) + pressureAt(i)) * 0.5f;
			const float segmentWidth = stroke.width * pressure;
			ofSetLineWidth(std::max(0.6f, segmentWidth * 0.22f));
			ofVec2f normal(-(end.y - start.y), end.x - start.x);
			if (normal.length() > 0.001f) {
				normal.normalize();
			}
			for (int bristle = -2; bristle <= 2; ++bristle) {
				ofVec2f offset = normal * (segmentWidth * 0.16f * bristle);
				ofDrawLine(start + offset, end + offset);
			}
		}
	} else if (stroke.tool == Tool::Pencil && !stroke.eraser) {
		for (std::size_t i = 1; i < stroke.points.size(); ++i) {
			const ofVec2f start = stroke.points[i - 1];
			const ofVec2f end = stroke.points[i];
			const float pressure = (pressureAt(i - 1) + pressureAt(i)) * 0.5f;
			ofSetLineWidth(std::max(0.6f, stroke.width * pressure * 0.7f));
			ofDrawLine(start, end);
			ofVec2f normal(-(end.y - start.y), end.x - start.x);
			if (normal.length() > 0.001f) {
				normal.normalize();
			}
			ofColor grainColor = color;
			grainColor.a = static_cast<unsigned char>(color.a * 0.25f);
			ofSetColor(grainColor);
			ofDrawLine(start + normal * 0.6f, end + normal * 0.6f);
			ofSetColor(color);
		}
	} else {
		std::vector<ofVec2f> smoothPoints = stroke.points;
		for (std::size_t i = 1; i + 1 < stroke.points.size(); ++i) {
			smoothPoints[i] = (stroke.points[i - 1] + stroke.points[i] * 2.0f +
				stroke.points[i + 1]) * 0.25f;
		}

		ofMesh mesh;
		mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
		for (std::size_t i = 0; i < smoothPoints.size(); ++i) {
			const ofVec2f previous = smoothPoints[i == 0 ? i : i - 1];
			const ofVec2f next = smoothPoints[i + 1 < smoothPoints.size() ? i + 1 : i];
			ofVec2f tangent = next - previous;
			if (tangent.length() <= 0.001f) {
				tangent.set(1.0f, 0.0f);
			} else {
				tangent.normalize();
			}
			ofVec2f normal(-tangent.y, tangent.x);
			const float halfWidth = stroke.width * pressureAt(i) * 0.5f;
			const ofVec2f left = smoothPoints[i] + normal * halfWidth;
			const ofVec2f right = smoothPoints[i] - normal * halfWidth;
			mesh.addVertex(ofVec3f(left.x, left.y, 0.0f));
			mesh.addColor(color);
			mesh.addVertex(ofVec3f(right.x, right.y, 0.0f));
			mesh.addColor(color);
		}
		mesh.draw();
		ofDrawCircle(smoothPoints.front(), stroke.width * pressureAt(0) * 0.5f);
		ofDrawCircle(smoothPoints.back(), stroke.width * pressureAt(stroke.points.size() - 1) * 0.5f);
	}
	ofPopStyle();
}

//--------------------------------------------------------------

void ofApp::drawStrokeLayer(const Stroke &stroke){
	Stroke opaqueStroke = stroke;
	if (!opaqueStroke.eraser) {
		opaqueStroke.color.a = 255;
	}
	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofEnableAlphaBlending();
	drawStroke(opaqueStroke);
	ofPopStyle();
	strokeLayer.end();
}

//--------------------------------------------------------------
void ofApp::renderStroke(const Stroke &stroke){
	applyStroke(activeLayer(), stroke);
	rebuildCanvas();
}

//--------------------------------------------------------------
void ofApp::applyStroke(Layer &layer, const Stroke &stroke){
	drawStrokeLayer(stroke);
	layer.image.begin();
	ofEnableAlphaBlending();
	if (stroke.eraser) {
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		ofSetColor(255);
		strokeLayer.draw(0, 0);
	} else {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		ofSetColor(stroke.color.a, stroke.color.a, stroke.color.a, stroke.color.a);
		strokeLayer.draw(0, 0);
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	layer.image.end();
	ofEnableAlphaBlending();
}

//--------------------------------------------------------------
void ofApp::rebuildLayer(Layer &layer){
	layer.image.begin();
	ofClear(0, 0, 0, 0);
	layer.image.end();
	for (const auto &stroke : layer.strokes) {
		applyStroke(layer, stroke);
	}
}

//--------------------------------------------------------------
void ofApp::rebuildCanvas(){
	canvas.begin();
	ofClear(paperColor);
	canvas.end();
	canvas.begin();
	ofEnableAlphaBlending();
	for (const auto &layer : layers) {
		if (layer->visible) {
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			const int opacity = static_cast<int>(layer->opacity * 255.0f);
			ofSetColor(opacity, opacity, opacity, opacity);
			layer->image.draw(0, 0);
		}
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	canvas.end();
}

//--------------------------------------------------------------
ofApp::Layer &ofApp::activeLayer(){
	return *layers[activeLayerIndex];
}

//--------------------------------------------------------------
void ofApp::addLayer(){
	auto layer = std::make_unique<Layer>();
	layer->name = "Layer " + ofToString(++nextLayerNumber);
	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	layer->image.allocate(settings);
	layer->image.begin();
	ofClear(0, 0, 0, 0);
	layer->image.end();
	layers.push_back(std::move(layer));
	activeLayerIndex = layers.size() - 1;
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::removeActiveLayer(){
	if (layers.size() <= 1) {
		clearCanvas();
		return;
	}
	layers.erase(layers.begin() + activeLayerIndex);
	activeLayerIndex = std::min(activeLayerIndex, layers.size() - 1);
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::moveActiveLayer(int direction){
	const int destination = static_cast<int>(activeLayerIndex) + direction;
	if (destination < 0 || destination >= static_cast<int>(layers.size())) {
		return;
	}
	std::swap(layers[activeLayerIndex], layers[destination]);
	activeLayerIndex = static_cast<std::size_t>(destination);
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::updateLayerPanelLayout(){
	const float panelWidth = 194.0f;
	const float panelX = std::max(8.0f, ofGetWidth() - panelWidth - 18.0f);
	const float rowsY = 84.0f;
	const float rowHeight = 23.0f;
	layerPanelBounds.set(panelX, 16, panelWidth,
		rowsY + layers.size() * rowHeight + 52.0f);
	layerAddBounds.set(panelX + 8, 50, 36, 24);
	layerRemoveBounds.set(panelX + 48, 50, 36, 24);
	layerUpBounds.set(panelX + 88, 50, 36, 24);
	layerDownBounds.set(panelX + 128, 50, 36, 24);
	layerRowBounds.clear();
	layerVisibilityBounds.clear();
	for (std::size_t row = 0; row < layers.size(); ++row) {
		const float rowY = rowsY + row * rowHeight;
		layerRowBounds.emplace_back(panelX + 8, rowY, panelWidth - 16, rowHeight - 2);
		layerVisibilityBounds.emplace_back(panelX + panelWidth - 38, rowY, 28, rowHeight - 2);
	}
	layerOpacityBounds.set(panelX + 12, rowsY + layers.size() * rowHeight + 22,
		panelWidth - 24, 8);
}

//--------------------------------------------------------------
void ofApp::drawLayerPanel(){
	updateLayerPanelLayout();
	ofPushStyle();
	ofSetColor(8, 15, 20, 238);
	ofDrawRectangle(layerPanelBounds);
	ofSetColor(28, 130, 142);
	ofDrawRectangle(layerPanelBounds.x, layerPanelBounds.y, layerPanelBounds.width, 28);
	ofSetColor(235, 242, 240);
	ofDrawBitmapString("LAYERS", layerPanelBounds.x + 10, layerPanelBounds.y + 19);

	auto drawButton = [](const ofRectangle &bounds, const std::string &label){
		ofSetColor(35, 57, 62);
		ofDrawRectangle(bounds);
		ofSetColor(230, 238, 236);
		ofDrawBitmapString(label, bounds.x + 10, bounds.y + 17);
	};
	drawButton(layerAddBounds, "+");
	drawButton(layerRemoveBounds, "-");
	drawButton(layerUpBounds, "UP");
	drawButton(layerDownBounds, "DN");

	for (std::size_t row = 0; row < layers.size(); ++row) {
		const std::size_t index = layers.size() - row - 1;
		const Layer &layer = *layers[index];
		ofSetColor(index == activeLayerIndex ? ofColor(35, 112, 115) : ofColor(20, 33, 38));
		ofDrawRectangle(layerRowBounds[row]);
		ofSetColor(230, 238, 236);
		ofDrawBitmapString(layer.name, layerRowBounds[row].x + 7, layerRowBounds[row].y + 15);
		ofSetColor(layer.visible ? ofColor(55, 135, 126) : ofColor(70, 75, 76));
		ofDrawRectangle(layerVisibilityBounds[row]);
		ofSetColor(245);
		ofDrawBitmapString(layer.visible ? "ON" : "OFF",
			layerVisibilityBounds[row].x + 3, layerVisibilityBounds[row].y + 15);
	}

	const int opacityPercent = static_cast<int>(activeLayer().opacity * 100.0f);
	ofSetColor(220, 230, 232);
	ofDrawBitmapString("Opacity " + ofToString(opacityPercent) + "%",
		layerPanelBounds.x + 10, layerOpacityBounds.y - 7);
	ofSetColor(44, 60, 64);
	ofDrawRectangle(layerOpacityBounds);
	ofSetColor(39, 164, 157);
	ofDrawRectangle(layerOpacityBounds.x, layerOpacityBounds.y,
		layerOpacityBounds.width * activeLayer().opacity, layerOpacityBounds.height);
	ofPopStyle();
}

//--------------------------------------------------------------
void ofApp::setActiveLayerOpacity(int x){
	activeLayer().opacity = ofClamp((x - layerOpacityBounds.x) / layerOpacityBounds.width, 0.0f, 1.0f);
	rebuildCanvas();
}

//--------------------------------------------------------------
void ofApp::fillAt(int x, int y){
	const int width = static_cast<int>(canvasBounds.width);
	const int height = static_cast<int>(canvasBounds.height);
	if (x < 0 || x >= width || y < 0 || y >= height) {
		return;
	}

	ofPixels pixels;
	canvas.readToPixels(pixels);
	const int pixelY = fillReadbackFlipped ? height - 1 - y : y;
	const ofColor target = pixels.getColor(x, pixelY);
	ofColor fillColor = brushColor;
	fillColor.a = brushOpacity;
	const int tolerance = fillTolerance;
	auto matches = [tolerance](const ofColor &left, const ofColor &right){
		return std::abs(static_cast<int>(left.r) - right.r) <= tolerance &&
			std::abs(static_cast<int>(left.g) - right.g) <= tolerance &&
			std::abs(static_cast<int>(left.b) - right.b) <= tolerance &&
			std::abs(static_cast<int>(left.a) - right.a) <= tolerance;
	};
	if (matches(target, fillColor)) {
		return;
	}

	const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
	std::vector<unsigned char> visited(pixelCount, 0);
	std::vector<unsigned char> filled(pixelCount, 0);
	std::vector<int> pending;
	pending.reserve(4096);
	auto enqueue = [&](int px, int py){
		if (px < 0 || px >= width || py < 0 || py >= height) {
			return;
		}
		const std::size_t index = static_cast<std::size_t>(py) * width + px;
		if (!visited[index]) {
			visited[index] = 1;
			pending.push_back(static_cast<int>(index));
		}
	};

	enqueue(x, pixelY);
	for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
		const int index = pending[cursor];
		const int px = index % width;
		const int py = index / width;
		if (!matches(pixels.getColor(px, py), target)) {
			continue;
		}
		filled[static_cast<std::size_t>(index)] = 1;
		enqueue(px - 1, py);
		enqueue(px + 1, py);
		enqueue(px, py - 1);
		enqueue(px, py + 1);
	}

	Stroke fill;
	fill.color = fillColor;
	fill.width = 1.0f;
	fill.tool = Tool::Fill;
	fill.eraser = false;
	for (int rawY = 0; rawY < height; ++rawY) {
		int px = 0;
		while (px < width) {
			const std::size_t index = static_cast<std::size_t>(rawY) * width + px;
			if (!filled[index]) {
				++px;
				continue;
			}
			const int start = px;
			while (px < width && filled[static_cast<std::size_t>(rawY) * width + px]) {
				++px;
			}
			const int drawY = fillReadbackFlipped ? height - 1 - rawY : rawY;
			fill.fillSpans.push_back({start, drawY, px - start});
		}
	}
	if (fill.fillSpans.empty()) {
		return;
	}

	Layer &layer = activeLayer();
	layer.strokes.push_back(std::move(fill));
	applyStroke(layer, layer.strokes.back());
	rebuildCanvas();
}

//--------------------------------------------------------------
bool ofApp::handleLayerPanelPress(int x, int y){
	updateLayerPanelLayout();
	if (!layerPanelBounds.inside(x, y)) {
		return false;
	}
	if (layerAddBounds.inside(x, y)) {
		addLayer();
	} else if (layerRemoveBounds.inside(x, y)) {
		removeActiveLayer();
	} else if (layerUpBounds.inside(x, y)) {
		moveActiveLayer(1);
	} else if (layerDownBounds.inside(x, y)) {
		moveActiveLayer(-1);
	} else if (layerOpacityBounds.inside(x, y)) {
		draggingLayerOpacity = true;
		setActiveLayerOpacity(x);
	} else {
		for (std::size_t row = 0; row < layerRowBounds.size(); ++row) {
			const std::size_t index = layers.size() - row - 1;
			if (layerVisibilityBounds[row].inside(x, y)) {
				layers[index]->visible = !layers[index]->visible;
				rebuildCanvas();
				break;
			}
			if (layerRowBounds[row].inside(x, y)) {
				activeLayerIndex = index;
				break;
			}
		}
	}
	return true;
}

//--------------------------------------------------------------
void ofApp::selectTool(Tool tool){
	selectedTool = tool;
	brushTool = tool == Tool::Brush;
	penTool = tool == Tool::Pen;
	pencilTool = tool == Tool::Pencil;
	markerTool = tool == Tool::Marker;
	paintbrushTool = tool == Tool::Paintbrush;
	fillTool = tool == Tool::Fill;
	eraser = false;
}

//--------------------------------------------------------------
void ofApp::brushToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Brush);
	else if (selectedTool == Tool::Brush) brushTool = true;
}

//--------------------------------------------------------------
void ofApp::penToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Pen);
	else if (selectedTool == Tool::Pen) penTool = true;
}

//--------------------------------------------------------------
void ofApp::pencilToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Pencil);
	else if (selectedTool == Tool::Pencil) pencilTool = true;
}

//--------------------------------------------------------------
void ofApp::markerToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Marker);
	else if (selectedTool == Tool::Marker) markerTool = true;
}

//--------------------------------------------------------------
void ofApp::paintbrushToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Paintbrush);
	else if (selectedTool == Tool::Paintbrush) paintbrushTool = true;
}

//--------------------------------------------------------------
void ofApp::fillToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Fill);
	else if (selectedTool == Tool::Fill) fillTool = true;
}

//--------------------------------------------------------------
void ofApp::undoLastStroke(){
	Layer &layer = activeLayer();
	if (!layer.strokes.empty()) {
		layer.strokes.pop_back();
		rebuildLayer(layer);
		rebuildCanvas();
	}
}

//--------------------------------------------------------------
void ofApp::clearCanvas(){
	activeLayer().strokes.clear();
	drawingStroke = false;
	pendingStroke = false;
	tabletStrokeInput = false;
	tabletStrokePositionInitialized = false;
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	rebuildLayer(activeLayer());
	rebuildCanvas();
}

//--------------------------------------------------------------
void ofApp::saveArtwork(){
	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	exportBuffer.allocate(settings);
	exportBuffer.begin();
	ofClear(paperColor);
	ofEnableAlphaBlending();
	canvas.draw(0, 0);
	exportBuffer.end();

	ofPixels pixels;
	exportBuffer.readToPixels(pixels);
	ofFileDialogResult result = ofSystemSaveDialog("Artwork.png", "Export canvas as PNG");
	if (result.bSuccess) {
		ofSaveImage(pixels, result.getPath());
	}
}

//--------------------------------------------------------------
void ofApp::finishActiveStroke(){
	if (!drawingStroke) {
		pendingStroke = false;
		tabletStrokeInput = false;
		tabletStrokePositionInitialized = false;
		return;
	}
	activeLayer().strokes.push_back(activeStroke);
	renderStroke(activeStroke);
	drawingStroke = false;
	pendingStroke = false;
	tabletStrokeInput = false;
	tabletStrokePositionInitialized = false;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if (key == '1') {
		selectTool(Tool::Brush);
	} else if (key == '2') {
		selectTool(Tool::Pen);
	} else if (key == '3') {
		selectTool(Tool::Pencil);
	} else if (key == '4') {
		selectTool(Tool::Marker);
	} else if (key == '5') {
		selectTool(Tool::Paintbrush);
	} else if (key == '6' || key == 'f' || key == 'F') {
		selectTool(Tool::Fill);
	} else if (key == 'e' || key == 'E') {
		eraser = true;
	} else if ((key == 'z' || key == 'Z') && ofGetKeyPressed(OF_KEY_CONTROL)) {
		undoLastStroke();
	} else if (key == 'c' || key == 'C') {
		clearCanvas();
	} else if (key == 's' || key == 'S') {
		saveArtwork();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	pollTabletPressure();
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
	pollTabletPressure();
	if (draggingLayerOpacity) {
		setActiveLayerOpacity(x);
		return;
	}
	if (tabletStrokeInput) {
		return;
	}
	if (!pendingStroke || !canvasBounds.inside(x, y)) {
		return;
	}
	const ofVec2f currentPoint(x - canvasBounds.x, y - canvasBounds.y);
	appendStrokePoint(currentPoint, 1.0f);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	pollTabletPressure();
	int pressX = x;
	int pressY = y;
	const bool tabletContact = tabletPressureSourceActive && tabletPressure > 0.02f &&
		tabletPointerPositionValid;
	if (tabletContact) {
		pressX = static_cast<int>(tabletPointerPosition.x);
		pressY = static_cast<int>(tabletPointerPosition.y);
	}
	if (handleLayerPanelPress(pressX, pressY) || gui.getShape().inside(pressX, pressY) ||
		!canvasBounds.inside(pressX, pressY)) {
		return;
	}
	const int localX = pressX - static_cast<int>(canvasBounds.x);
	const int localY = pressY - static_cast<int>(canvasBounds.y);
	if (selectedTool == Tool::Fill && !eraser) {
		fillAt(localX, localY);
		return;
	}
	drawingStroke = false;
	pendingStroke = true;
	pendingMotionSamples = 0;
	tabletStrokeInput = tabletContact;
	tabletStrokePositionInitialized = tabletContact;
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	activeStroke.fillSpans.clear();
	activeStroke.points.emplace_back(localX, localY);
	activeStroke.pressures.push_back(tabletContact ? tabletPressure : 1.0f);
	pendingStrokeStartedAt = ofGetElapsedTimeMillis();
	activeStroke.color = brushColor;
	activeStroke.color.a = brushOpacity;
	activeStroke.tool = selectedTool == Tool::Fill ? Tool::Brush : selectedTool;
	activeStroke.width = brushSize;
	if (!eraser) {
		switch (selectedTool) {
			case Tool::Pen:
				activeStroke.width *= 0.4f;
				break;
			case Tool::Pencil:
				activeStroke.width *= 0.55f;
				activeStroke.color.a = static_cast<unsigned char>(activeStroke.color.a * 0.55f);
				break;
			case Tool::Marker:
				activeStroke.width *= 1.7f;
				activeStroke.color.a = static_cast<unsigned char>(activeStroke.color.a * 0.32f);
				break;
			case Tool::Paintbrush:
				activeStroke.width *= 1.35f;
				break;
			case Tool::Brush:
			case Tool::Fill:
				break;
		}
	}
	activeStroke.eraser = eraser;
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
	if (draggingLayerOpacity) {
		draggingLayerOpacity = false;
		return;
	}
	pollTabletPressure();
	if (pendingStroke && drawingStroke) {
		if (!tabletStrokeInput && canvasBounds.inside(x, y)) {
			const ofVec2f endPoint(x - canvasBounds.x, y - canvasBounds.y);
			if ((endPoint - activeStroke.points.back()).length() > 0.5f) {
				activeStroke.points.push_back(endPoint);
				activeStroke.pressures.push_back(1.0f);
			}
		}
		finishActiveStroke();
	} else if (pendingStroke) {
		pendingStroke = false;
		activeStroke.points.clear();
		activeStroke.pressures.clear();
		tabletStrokeInput = false;
		tabletStrokePositionInitialized = false;
	}
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
	finishActiveStroke();
	pendingStroke = false;
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
	resizeCanvas(w, h);
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
