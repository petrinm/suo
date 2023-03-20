#pragma once

// http://schneegans.github.io/tutorials/2015/09/20/signal-slot

#include <functional>
#include <map>

namespace suo {

// A Port object may call multiple slots with the
// same signature. You can connect functions to the Port
// which will be called when the emit() method on the
// Port object is invoked. Any argument passed to emit()
// will be passed to the given functions.

template <typename... Args>
class Port {

public:
	Port() = default;
	~Port() = default;

	// Copy constructor and assignment create a new Port.
	Port(Port const& /*unused*/) {}

	Port& operator=(Port const& other) {
		if (this != &other) {
			disconnect_all();
		}
		return *this;
	}

	// Move constructor and assignment operator work as expected.
	Port(Port&& other) noexcept :
		_slots(std::move(other._slots)),
		_current_id(other._current_id) {}

	Port& operator=(Port&& other) noexcept {
		if (this != &other) {
			_slots = std::move(other._slots);
			_current_id = other._current_id;
		}

		return *this;
	}


	// Connects a std::function to the Port. The returned
	// value can be used to disconnect the function again.
	int connect(std::function<void(Args...)> const& slot) const {
		_slots.insert(std::make_pair(++_current_id, slot));
		return _current_id;
	}

	// Convenience method to connect a member function of an
	// object to this Port.
	template <typename T>
	int connect_member(T* inst, void (T::* func)(Args...)) {
		return connect([=](Args... args) {
			(inst->*func)(args...);
			});
	}

	// Convenience method to connect a const member function
	// of an object to this Port.
	template <typename T>
	int connect_member(T* inst, void (T::* func)(Args...) const) {
		return connect([=](Args... args) {
			(inst->*func)(args...);
			});
	}

	// Disconnects a previously connected function.
	void disconnect(int id) const {
		_slots.erase(id);
	}

	// Disconnects all previously connected functions.
	void disconnect_all() const {
		_slots.clear();
	}

	// Calls all connected functions.
	void emit(Args... p) {
		for (auto const& it : _slots) {
			it.second(p...);
		}
	}

	// Calls all connected functions except for one.
	void emit_for_all_but_one(int excludedConnectionID, Args... p) {
		for (auto const& it : _slots) {
			if (it.first != excludedConnectionID) {
				it.second(p...);
			}
		}
	}

	// Calls only one connected function.
	void emit_for(int connectionID, Args... p) {
		auto const& it = _slots.find(connectionID);
		if (it != _slots.end()) {
			it->second(p...);
		}
	}

	bool has_connections() const {
		return _slots.empty() == false;
	}

private:
	mutable std::map<int, std::function<void(Args...)>> _slots;
	mutable int _current_id{ 0 };
};



template <typename Ret, typename... Args>
class SourcePort {

public:
	SourcePort() = default;
	~SourcePort() = default;

	// Copy constructor and assignment create a new Port.
	SourcePort(SourcePort const& /*unused*/) {}

	SourcePort& operator=(SourcePort const& other) {
		if (this != &other) {
			disconnect_all();
		}
		return *this;
	}

	// Move constructor and assignment operator work as expected.
	SourcePort(SourcePort&& other) noexcept:
		_slots(std::move(other._slots)),
		_current_id(other._current_id) {}

	SourcePort& operator=(SourcePort&& other) noexcept {
		if (this != &other) {
			_slots = std::move(other._slots);
			_current_id = other._current_id;
		}

		return *this;
	}


	// Connects a std::function to the SourcePort. The returned
	// value can be used to disconnect the function again.
	int connect(std::function<Ret(Args...)> const& slot) const {
		_slots.insert(std::make_pair(++_current_id, slot));
		return _current_id;
	}

	// Convenience method to connect a member function of an
	// object to this SourcePort.
	template <typename T>
	int connect_member(T* inst, Ret (T::* func)(Args...)) {
		return connect([=](Args... args) -> Ret {
			return (inst->*func)(args...);
			});
	}

	// Convenience method to connect a const member function
	// of an object to this SourcePort.
	template <typename T>
	int connect_member(T* inst, Ret (T::* func)(Args...) const) {
		return connect([=](Args... args) -> Ret {
			return (inst->*func)(args...);
			});
	}

	// Disconnects a previously connected function.
	void disconnect(int id) const {
		_slots.erase(id);
	}

	// Disconnects all previously connected functions.
	void disconnect_all() const {
		_slots.clear();
	}

	// Calls all connected functions.
	Ret emit(Args... p) {
		for (auto const& it : _slots) {
			Ret ret = it.second(p...);
			if (ret)
				return ret;
		}
		return Ret();
	}

	// Calls only one connected function.
	Ret emit_for(int connectionID, Args... p) {
		auto const& it = _slots.find(connectionID);
		if (it != _slots.end()) {
			return it->second(p...);
		}
	}

	bool has_connections() const {
		return _slots.empty() == false;
	}

private:
	mutable std::map<int, std::function<Ret(Args...)>> _slots;
	mutable int _current_id{ 0 };
};


}; // namespace
