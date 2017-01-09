#ifndef __optional_h__
#define __optional_h__

#include <type_traits>
#include <stdexcept>

class bad_optional_access : public std::exception {
public:
	virtual const char* what() const noexcept {
		return "bad optional access";
	}
};

template<class T>
class optional {
public:
	typedef T value_type;	

	optional() = default;

	optional( const optional& other ) {
		if (other._engaged) {
			new (std::addressof(_data)) T(*other);
			_engaged = true;
		}
	}

	optional( const optional&& other ) {
		if (other._engaged) {
			new (std::addressof(_data)) T(std::move(*other));
			_engaged = true;
		}
	}

	template < class U >
	optional( const optional<U>& other ) {
		if (other._engaged) {
			new (std::addressof(_data)) T(*other);
			_engaged = true;
		}
	}

	template < class U >
	optional( optional<U>&& other ) {
		if (other._engaged) {
			new (std::addressof(_data)) T(std::move(*other));
			_engaged = true;
		}
	}


	template<class U = T>
	optional(U &&value) {
		new(std::addressof(_data)) T(std::forward<U>(value));
		_engaged = true;
	}



	~optional() {
		reset();
	}


	constexpr explicit operator bool() const {
		return _engaged;
	}

	constexpr bool has_value() const {
		return _engaged;
	}

	/* these should throw ... */
	constexpr T& value() & {
		if (!_engaged) throw bad_optional_access();
		return *reinterpret_cast<T*>(std::addressof(_data));
	}

	constexpr const T & value() const & {
		if (!_engaged) throw bad_optional_access();
		return *reinterpret_cast<const T*>(std::addressof(_data));
	}

	constexpr T&& value() && {
		if (!_engaged) throw bad_optional_access();
		return *reinterpret_cast<T*>(std::addressof(_data));
	}

	constexpr const T&& value() const && {
		if (!_engaged) throw bad_optional_access();
		return *reinterpret_cast<const T*>(std::addressof(_data));
	}


	template< class U > 
	constexpr T value_or( U&& default_value ) const& {
		return bool(*this) ? **this : static_cast<T>(std::forward<U>(default_value));
	}

	template< class U > 
	constexpr T value_or( U&& default_value ) && {
		return bool(*this) ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
	}

	constexpr const T* operator->() const {
		return reinterpret_cast<const T*>(std::addressof(_data));
	}
	constexpr T* operator->() {
		return reinterpret_cast<T*>(std::addressof(_data));
	}

	constexpr const T& operator*() const& {
		return *reinterpret_cast<const T*>(std::addressof(_data));
	}
	constexpr T& operator*() & {
		return *reinterpret_cast<T*>(std::addressof(_data));
	}
	constexpr const T&& operator*() const&& {
		return *reinterpret_cast<const T*>(std::addressof(_data));
	}
	constexpr T&& operator*() && {
		return *reinterpret_cast<T*>(std::addressof(_data));
	}


	void reset() {
		if (_engaged) {
			reinterpret_cast<const T*>(std::addressof(_data))->~T();
			_engaged = false;
		}
	}

private:
	bool _engaged = false;
	typename std::aligned_storage<sizeof(T), alignof(T)>::type _data;
};

#endif

