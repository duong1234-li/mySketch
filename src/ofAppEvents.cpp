#include "ofApp.h"

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
	} else if (key == '7') {
		selectTool(Tool::MoveResize);
	} else if (key == 'e' || key == 'E') {
		eraser = true;
	} else if ((key == 'z' || key == 'Z') && ofGetKeyPressed(OF_KEY_CONTROL)) {
		undoLastStroke();
	} else if (key == 'c' || key == 'C') {
		clearCanvas();
	} else if (key == 's' || key == 'S') {
		saveArtwork();
	} else if (key == 'i' || key == 'I') {
		ofFileDialogResult result = ofSystemLoadDialog("Load image to active layer");
		if (result.bSuccess) {
			addImageToActiveLayer(result.getPath());
		}
	} else if (key == 'd' || key == 'D') {
		removeImageFromActiveLayer();
	} else if (key == 'r' || key == 'R') {
		resetImageTransformForActiveLayer();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	pollTabletPressure();
	{
		ofMouseEventArgs args(ofMouseEventArgs::Moved, x, y);
		layersPanel.mouseMoved(args);
	}
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
	pollTabletPressure();
	if (draggingLayerOpacity) {
		setActiveLayerOpacity(x);
		return;
	}
	{
		ofMouseEventArgs args(ofMouseEventArgs::Dragged, x, y, button);
		layersPanel.mouseDragged(args);
	}
	if (draggingLayerTransform) {
		const ofVec2f mousePos = screenToCanvasPoint(x, y);
		Layer &layer = activeLayer();
		if (resizingLayerTransform) {
			const ofVec2f startVector = transformResizeStart - transformResizeAnchor;
			const ofVec2f currentVector = mousePos - transformResizeAnchor;
			const float lengthSquared = startVector.x * startVector.x + startVector.y * startVector.y;
			const float factor = lengthSquared > 0.001f
				? (currentVector.x * startVector.x + currentVector.y * startVector.y) / lengthSquared
				: 1.0f;
			const float minScale = selectedLayerItemType == LayerItemType::Layer ? 0.1f : 0.05f;
			const float maxScale = selectedLayerItemType == LayerItemType::Layer ? 8.0f : 20.0f;
			const float newScale = ofClamp(transformResizeStartScale * factor, minScale, maxScale);
			if (selectedLayerItemType == LayerItemType::Layer) {
				layer.transformScale = newScale;
				layer.transformPosition = transformResizeAnchor - transformResizeParentAnchor * newScale;
			} else if (selectedLayerItemType == LayerItemType::Image && layer.hasImage) {
				layer.imageScale = newScale;
				layer.imagePosition = transformResizeParentAnchor - transformResizeLocalAnchor * newScale;
			}
		} else {
			if (selectedLayerItemType == LayerItemType::Layer) {
				layer.transformPosition = layerTransformPositionStart + (mousePos - layerTransformDragStart);
			} else {
				const ofVec2f layerPoint = canvasPointToLayer(layer, mousePos);
				const ofVec2f delta = layerPoint - layerTransformDragStart;
				if (selectedLayerItemType == LayerItemType::Image && layer.hasImage) {
					layer.imagePosition = layerTransformPositionStart + delta;
				}
			}
		}
		if (selectedLayerItemType != LayerItemType::Layer) {
			rebuildLayer(layer);
		}
		rebuildCanvas();
		return;
	}
	if (!pendingStroke || !canvasBounds.inside(x, y)) {
		return;
	}
	const ofVec2f currentPoint = canvasPointToLayer(activeLayer(), screenToCanvasPoint(x, y));
	appendStrokePoint(currentPoint, effectivePressure());
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	pollTabletPressure();
	{
		ofMouseEventArgs args(ofMouseEventArgs::Pressed, x, y, button);
		layersPanel.mousePressed(args);
	}
	if (handleLayerPanelPress(x, y)) {
		return;
	}
	if (gui.getShape().inside(x, y) || layersPanel.getShape().inside(x, y) || !canvasBounds.inside(x, y)) {
		return;
	}
	const ofVec2f canvasPoint = screenToCanvasPoint(x, y);
	const int localX = static_cast<int>(canvasPoint.x);
	const int localY = static_cast<int>(canvasPoint.y);
	Layer &active = activeLayer();
	if (selectedTool == Tool::MoveResize) {
		draggingLayerTransform = true;
		resizingLayerTransform = false;
		const ofRectangle content = selectedContentBounds(active);
		if (content.width > 0.0f && content.height > 0.0f) {
			const ofRectangle bounds(
				active.transformPosition.x + content.x * active.transformScale,
				active.transformPosition.y + content.y * active.transformScale,
				content.width * active.transformScale,
				content.height * active.transformScale);
			const ofVec2f corners[] = {
				ofVec2f(bounds.x, bounds.y),
				ofVec2f(bounds.x + bounds.width, bounds.y),
				ofVec2f(bounds.x + bounds.width, bounds.y + bounds.height),
				ofVec2f(bounds.x, bounds.y + bounds.height)
			};
			const ofVec2f anchors[] = {
				ofVec2f(content.x + content.width, content.y + content.height),
				ofVec2f(content.x, content.y + content.height),
				ofVec2f(content.x, content.y),
				ofVec2f(content.x + content.width, content.y)
			};
			const float handleCanvasSize = 16.0f / canvasViewScale;
			for (int corner = 0; corner < 4; ++corner) {
				const ofRectangle handle(corners[corner].x - handleCanvasSize * 0.5f,
					corners[corner].y - handleCanvasSize * 0.5f,
					handleCanvasSize, handleCanvasSize);
				if (handle.inside(canvasPoint.x, canvasPoint.y)) {
					resizingLayerTransform = true;
					transformResizeStart = canvasPoint;
					transformResizeParentAnchor = anchors[corner];
					transformResizeLocalAnchor = anchors[corner];
					transformResizeStartScale = active.transformScale;
					if (selectedLayerItemType == LayerItemType::Image && active.hasImage) {
						const ofVec2f imageAnchors[] = {
							ofVec2f(active.layerImage.getWidth(), active.layerImage.getHeight()),
							ofVec2f(0, active.layerImage.getHeight()),
							ofVec2f(0, 0),
							ofVec2f(active.layerImage.getWidth(), 0)
						};
						transformResizeLocalAnchor = imageAnchors[corner];
						transformResizeParentAnchor = active.imagePosition +
							transformResizeLocalAnchor * active.imageScale;
						transformResizeStartScale = active.imageScale;
					}
					transformResizeAnchor = active.transformPosition +
						transformResizeParentAnchor * active.transformScale;
					return;
				}
			}
		}
		if (selectedLayerItemType == LayerItemType::Layer) {
			layerTransformDragStart = canvasPoint;
			layerTransformPositionStart = active.transformPosition;
		} else {
			layerTransformDragStart = canvasPointToLayer(active, canvasPoint);
			if (selectedLayerItemType == LayerItemType::Image && active.hasImage) {
				layerTransformPositionStart = active.imagePosition;
			}
		}
		return;
	}
	const ofVec2f layerPoint = canvasPointToLayer(active, canvasPoint);
	if (selectedTool == Tool::Fill && !eraser) {
		fillAt(localX, localY);
		return;
	}
	drawingStroke = false;
	pendingStroke = true;
	pendingMotionSamples = 0;
	resetSimulatedPressure();
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	activeStroke.fillSpans.clear();
	activeStroke.points.emplace_back(layerPoint);
	activeStroke.pressures.push_back(effectivePressure());
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
			case Tool::MoveResize:
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
	{
		ofMouseEventArgs args(ofMouseEventArgs::Released, x, y, button);
		layersPanel.mouseReleased(args);
	}
	if (draggingLayerTransform) {
		draggingLayerTransform = false;
		resizingLayerTransform = false;
		return;
	}
	pollTabletPressure();
	if (pendingStroke && drawingStroke) {
		if (canvasBounds.inside(x, y)) {
			const ofVec2f endPoint = canvasPointToLayer(activeLayer(), screenToCanvasPoint(x, y));
			if ((endPoint - activeStroke.points.back()).length() > 0.5f) {
				activeStroke.points.push_back(endPoint);
				activeStroke.pressures.push_back(effectivePressure());
			}
		}
		finishActiveStroke();
	} else if (pendingStroke) {
		pendingStroke = false;
		activeStroke.points.clear();
		activeStroke.pressures.clear();
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
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){
	(void)scrollX;
	if (layerPanelListBounds.inside(x, y) && scrollY != 0.0f) {
		layerPanelScrollOffset -= scrollY * 22.0f;
		updateLayerPanelLayout();
		return;
	}
	if (!canvasBounds.inside(x, y) || selectedTool != Tool::MoveResize || scrollY == 0.0f) {
		return;
	}
	Layer &active = activeLayer();
	const ofVec2f mouseCanvas = screenToCanvasPoint(x, y);
	const float scaleFactor = scrollY > 0 ? 1.1f : 0.9f;
	if (selectedLayerItemType == LayerItemType::Layer) {
		const float newScale = ofClamp(active.transformScale * scaleFactor, 0.1f, 8.0f);
		const float actualFactor = newScale / active.transformScale;
		active.transformPosition = mouseCanvas - (mouseCanvas - active.transformPosition) * actualFactor;
		active.transformScale = newScale;
	} else {
		const ofVec2f mouseLocal = canvasPointToLayer(active, mouseCanvas);
		if (selectedLayerItemType == LayerItemType::Image && active.hasImage) {
			const float newScale = ofClamp(active.imageScale * scaleFactor, 0.05f, 20.0f);
			const float actualFactor = newScale / active.imageScale;
			active.imagePosition = mouseLocal - (mouseLocal - active.imagePosition) * actualFactor;
			active.imageScale = newScale;
		}
		rebuildLayer(active);
	}
	rebuildCanvas();
}

void ofApp::dragEvent(ofDragInfo dragInfo){
	if (dragInfo.files.empty()) {
		return;
	}
	const std::string &path = dragInfo.files[0];
	addImageToActiveLayer(path);
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}
