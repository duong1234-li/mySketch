#include "ofApp.h"

#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
#include <libinput.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace {

// A Bluetooth tablet can pair or unpair while the sketch is open, so a missing
// pen is retried on a slow timer instead of only at startup.
const uint64_t tabletRescanIntervalMillis = 2000;

}

//--------------------------------------------------------------
// libinput opens every candidate event node through this callback, which is also
// where a missing /dev/input permission becomes visible.
int ofApp::openTabletDevice(const char *path, int flags, void *userData){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	auto *self = static_cast<ofApp *>(userData);
	const int fd = ::open(path, flags | O_CLOEXEC);
	if (fd >= 0) {
		return fd;
	}
	const int error = errno;
	if (error == EACCES || error == EPERM) {
		self->libinputPermissionDenied = true;
	}
	return -error;
#else
	(void)path;
	(void)flags;
	(void)userData;
	return -1;
#endif
}

//--------------------------------------------------------------
void ofApp::closeTabletDevice(int fd, void *userData){
	(void)userData;
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	::close(fd);
#else
	(void)fd;
#endif
}

//--------------------------------------------------------------
// libinput is the only usable pressure source on this XWayland session:
// XInput2 subscribes fine but never delivers a valuator, and the Wacom
// position axes are reported in tablet space that no client can map to the
// screen. Stroke positions therefore keep coming from the window mouse
// events, and libinput contributes pressure only.
void ofApp::setupTabletPressureInput(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	const libinput_interface tabletInterface = { openTabletDevice, closeTabletDevice };
	libinputContext = libinput_path_create_context(&tabletInterface, this);
	if (!libinputContext) {
		ofLogWarning("ofApp") << "Could not create a libinput context; using simulated pressure";
		return;
	}
	discoverTabletDevice();
	if (!tabletDeviceFound) {
		if (libinputPermissionDenied) {
			ofLogNotice("ofApp") << "No read access to /dev/input; using simulated pressure";
		} else {
			ofLogNotice("ofApp") << "No pressure-capable tablet detected; using simulated pressure";
		}
	}
#endif
}

//--------------------------------------------------------------
void ofApp::discoverTabletDevice(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (!libinputContext) {
		return;
	}
	libinputRescanAt = ofGetElapsedTimeMillis() + tabletRescanIntervalMillis;
	DIR *directory = ::opendir("/dev/input");
	if (!directory) {
		ofLogWarning("ofApp") << "Could not open /dev/input while looking for a tablet";
		return;
	}
	std::vector<std::string> candidates;
	while (const dirent *entry = ::readdir(directory)) {
		if (std::strncmp(entry->d_name, "event", 5) == 0) {
			candidates.emplace_back(std::string("/dev/input/") + entry->d_name);
		}
	}
	::closedir(directory);
	std::sort(candidates.begin(), candidates.end());

	for (const auto &path : candidates) {
		if (std::find(libinputDevicePaths.begin(), libinputDevicePaths.end(), path) !=
			libinputDevicePaths.end()) {
			continue;
		}
		if (::access(path.c_str(), R_OK) != 0) {
			if (errno == EACCES || errno == EPERM) {
				libinputPermissionDenied = true;
			}
			continue;
		}
		struct libinput_device *device = libinput_path_add_device(libinputContext, path.c_str());
		if (!device) {
			continue;
		}
		libinputDevicePaths.push_back(path);
		if (tabletDeviceFound ||
			!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_TOOL)) {
			continue;
		}
		// The handle returned by libinput_path_add_device is only valid until the
		// next dispatch, so the pen needs its own reference.
		libinputDevice = libinput_device_ref(device);
		libinputDevicePath = path;
		tabletDeviceFound = true;
		const char *name = libinput_device_get_name(device);
		ofLogNotice("ofApp") << "Pen pressure source: " << (name ? name : "unknown tablet")
			<< " on " << path;
	}
#endif
}

//--------------------------------------------------------------
void ofApp::pollTabletPressure(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (libinputContext && !tabletDeviceFound &&
		ofGetElapsedTimeMillis() >= libinputRescanAt) {
		discoverTabletDevice();
	} else if (libinputContext && tabletDeviceFound) {
		pollfd tabletFd = { libinput_get_fd(libinputContext), POLLIN, 0 };
		if (::poll(&tabletFd, 1, 0) > 0) {
			drainTabletEvents();
		}
	}
#endif
	if (!tabletPressureSourceActive) {
		updateSimulatedPressure();
	}
}

//--------------------------------------------------------------
void ofApp::drainTabletEvents(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (libinput_dispatch(libinputContext) < 0) {
		return;
	}
	struct libinput_event *event = nullptr;
	while ((event = libinput_get_event(libinputContext)) != nullptr) {
		applyTabletEvent(event);
		libinput_event_destroy(event);
	}
#endif
}

//--------------------------------------------------------------
void ofApp::applyTabletEvent(struct libinput_event *event){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	const enum libinput_event_type type = libinput_event_get_type(event);
	struct libinput_device *device = libinput_event_get_device(event);
	if (type == LIBINPUT_EVENT_DEVICE_REMOVED) {
		handleTabletDeviceRemoved(device);
		return;
	}
	// The tablet's pad and any mouse on the same bus share this context.
	if (!libinputDevice || device != libinputDevice) {
		return;
	}

	if (type == LIBINPUT_EVENT_TABLET_TOOL_AXIS) {
		struct libinput_event_tablet_tool *tool = libinput_event_get_tablet_tool_event(event);
		const float pressure = static_cast<float>(libinput_event_tablet_tool_get_pressure(tool));
		if (pressure > tabletContactThreshold) {
			tabletPressure = pressure;
			tabletPressureSourceActive = true;
		} else {
			endTabletContact();
		}
	} else if (type == LIBINPUT_EVENT_TABLET_TOOL_TIP ||
		type == LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY) {
		struct libinput_event_tablet_tool *tool = libinput_event_get_tablet_tool_event(event);
		const bool inContact = type == LIBINPUT_EVENT_TABLET_TOOL_TIP
			? libinput_event_tablet_tool_get_tip_state(tool) != 0
			: libinput_event_tablet_tool_get_proximity_state(tool) != 0;
		if (!inContact) {
			endTabletContact();
		}
	}
#endif
}

//--------------------------------------------------------------
// Lifting the pen mid-stroke must not zero the pressure: the release sample that
// the window reports afterwards would otherwise draw a hairline tail instead of
// finishing at the width the stroke had. The held value is dropped as soon as
// the stroke ends or a new one starts.
void ofApp::endTabletContact(){
	tabletPressureSourceActive = false;
	tabletPressureHeld = pendingStroke;
}

//--------------------------------------------------------------
void ofApp::handleTabletDeviceRemoved(struct libinput_device *device){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (!libinputDevice || device != libinputDevice) {
		return;
	}
	ofLogNotice("ofApp") << "Tablet " << libinputDevicePath << " went away; looking for it again";
	libinput_device_unref(libinputDevice);
	libinputDevice = nullptr;
	libinputDevicePaths.erase(
		std::remove(libinputDevicePaths.begin(), libinputDevicePaths.end(), libinputDevicePath),
		libinputDevicePaths.end());
	libinputDevicePath.clear();
	tabletDeviceFound = false;
	endTabletContact();
#endif
}

//--------------------------------------------------------------
void ofApp::exit(){
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
	if (libinputDevice) {
		libinput_device_unref(libinputDevice);
		libinputDevice = nullptr;
	}
	if (libinputContext) {
		libinput_unref(libinputContext);
		libinputContext = nullptr;
	}
#endif
}

//--------------------------------------------------------------
void ofApp::resetSimulatedPressure(){
	tabletPressureHeld = false;
	if (tabletPressureSourceActive) {
		return;
	}
	simulatedPressure = ofClamp(pressureSimulation, 0.0f, 1.0f);
	simulatedPressureValid = false;
	tabletPressure = simulatedPressure;
}

//--------------------------------------------------------------
void ofApp::updateSimulatedPressure(){
	if (tabletPressureSourceActive || tabletPressureHeld) {
		return;
	}
	const float ceiling = ofClamp(pressureSimulation, 0.0f, 1.0f);
	const ofVec2f position(ofGetMouseX(), ofGetMouseY());
	const uint64_t now = ofGetElapsedTimeMillis();
	if (!simulatedPressureValid || now <= simulatedPressureUpdatedAt) {
		simulatedPressurePosition = position;
		simulatedPressureUpdatedAt = now;
		simulatedPressure = ceiling;
		simulatedPressureValid = true;
		tabletPressure = simulatedPressure;
		return;
	}

	const float travel = (position - simulatedPressurePosition).length();
	const float elapsed = static_cast<float>(now - simulatedPressureUpdatedAt);
	const float speed = elapsed > 0.0f ? travel / elapsed : 0.0f;
	simulatedPressurePosition = position;
	simulatedPressureUpdatedAt = now;

	const float target = ceiling * ofClamp(1.0f - (speed - 0.15f) / 1.6f, 0.15f, 1.0f);
	simulatedPressure += (target - simulatedPressure) * 0.3f;
	tabletPressure = simulatedPressure;
}

//--------------------------------------------------------------
float ofApp::effectivePressure() const{
	// Floored so the faintest possible contact still leaves a hairline instead of
	// a zero-width mesh that renders as nothing at all.
	return ofClamp(tabletPressure * pressureSensitivity, 0.06f, 1.0f);
}
