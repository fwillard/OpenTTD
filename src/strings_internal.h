/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_interal.h Types and functions related to the internal workings of formatting OpenTTD's strings. */

#ifndef STRINGS_INTERNAL_H
#define STRINGS_INTERNAL_H

#include "strings_func.h"
#include "string_func.h"

class StringParameters {
	StringParameters *parent; ///< If not nullptr, this instance references data from this parent instance.
	uint64 *data;             ///< Array with the actual data.
	WChar *type;              ///< Array with type information about the data. Can be nullptr when no type information is needed. See #StringControlCode.

public:
	uint offset;              ///< Current offset in the data/type arrays.
	uint num_param;           ///< Length of the data array.
	WChar next_type = 0; ///< The type of the next data that is retrieved.

	/** Create a new StringParameters instance. */
	StringParameters(uint64 *data, uint num_param, WChar *type) :
		parent(nullptr),
		data(data),
		type(type),
		offset(0),
		num_param(num_param)
	{ }

	/** Create a new StringParameters instance. */
	template <size_t Tnum_param>
	StringParameters(int64 (&data)[Tnum_param]) :
		parent(nullptr),
		data((uint64 *)data),
		type(nullptr),
		offset(0),
		num_param(Tnum_param)
	{
		static_assert(sizeof(data[0]) == sizeof(uint64));
	}

	/**
	 * Create a new StringParameters instance that can reference part of the data of
	 * the given partent instance.
	 */
	StringParameters(StringParameters &parent, uint size) :
		parent(&parent),
		data(parent.data + parent.offset),
		offset(0),
		num_param(size)
	{
		assert(size <= parent.GetDataLeft());
		if (parent.type == nullptr) {
			this->type = nullptr;
		} else {
			this->type = parent.type + parent.offset;
		}
	}

	~StringParameters()
	{
		if (this->parent != nullptr) {
			this->parent->offset += this->num_param;
		}
	}

	void ClearTypeInformation();

	int64 GetInt64();

	/** Read an int32 from the argument array. @see GetInt64. */
	int32 GetInt32()
	{
		return (int32)this->GetInt64();
	}

	/**
	 * Get a new instance of StringParameters that is a "range" into the
	 * parameters existing parameters. Upon destruction the offset in the parent
	 * is not updated. However, calls to SetDParam do update the parameters.
	 *
	 * The returned StringParameters must not outlive this StringParameters.
	 * @return A "range" of the string parameters.
	 */
	StringParameters GetRemainingParameters()
	{
		return StringParameters(&this->data[this->offset], GetDataLeft(),
			this->type == nullptr ? nullptr : &this->type[this->offset]);
	}

	/** Return the amount of elements which can still be read. */
	uint GetDataLeft() const
	{
		return this->num_param - this->offset;
	}

	/** Get a pointer to a specific element in the data array. */
	uint64 *GetPointerToOffset(uint offset) const
	{
		assert(offset < this->num_param);
		return &this->data[offset];
	}

	/** Does this instance store information about the type of the parameters. */
	bool HasTypeInformation() const
	{
		return this->type != nullptr;
	}

	/** Get the type of a specific element. */
	WChar GetTypeAtOffset(uint offset) const
	{
		assert(offset < this->num_param);
		assert(this->HasTypeInformation());
		return this->type[offset];
	}

	void SetParam(uint n, uint64 v)
	{
		assert(n < this->num_param);
		this->data[n] = v;
	}

	uint64 GetParam(uint n) const
	{
		assert(n < this->num_param);
		return this->data[n];
	}
};

/**
 * Equivalent to the std::back_insert_iterator in function, with some
 * convenience helpers for string concatenation.
 */
class StringBuilder {
	std::string *string;

public:
	/* Required type for this to be an output_iterator; mimics std::back_insert_iterator. */
	using value_type = void;
	using difference_type = void;
	using iterator_category = std::output_iterator_tag;
	using pointer = void;
	using reference = void;

	/**
	 * Create the builder of an external buffer.
	 * @param start The start location to write to.
	 * @param last  The last location to write to.
	 */
	StringBuilder(std::string &string) : string(&string) {}

	/* Required operators for this to be an output_iterator; mimics std::back_insert_iterator, which has no-ops. */
	StringBuilder &operator++() { return *this; }
	StringBuilder operator++(int) { return *this; }
	StringBuilder &operator*() { return *this; }

	/**
	 * Operator to add a character to the end of the buffer. Like the back
	 * insert iterators this also increases the position of the end of the
	 * buffer.
	 * @param value The character to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator=(const char value)
	{
		return this->operator+=(value);
	}

	/**
	 * Operator to add a character to the end of the buffer.
	 * @param value The character to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(const char value)
	{
		this->string->push_back(value);
		return *this;
	}

	/**
	 * Operator to append the given string to the output buffer.
	 * @param str The string to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(std::string_view str)
	{
		*this->string += str;
		return *this;
	}

	/**
	 * Encode the given Utf8 character into the output buffer.
	 * @param c The character to encode.
	 */
	void Utf8Encode(WChar c)
	{
		auto iterator = std::back_inserter(*this->string);
		::Utf8Encode(iterator, c);
	}

	/**
	 * Remove the given amount of characters from the back of the string.
	 * @param amount The amount of characters to remove.
	 * @return true iff there was enough space and the character got added.
	 */
	void RemoveElementsFromBack(size_t amount)
	{
		this->string->erase(this->string->size() - std::min(amount, this->string->size()));
	}

	/**
	 * Get the current index in the string.
	 * @return The index.
	 */
	size_t CurrentIndex()
	{
		return this->string->size();
	}

	/**
	 * Get the reference to the character at the given index.
	 * @return The reference to the character.
	 */
	char &operator[](size_t index)
	{
		return (*this->string)[index];
	}
};

void GetStringWithArgs(StringBuilder &builder, StringID string, StringParameters &args, uint case_index = 0, bool game_script = false);
std::string GetStringWithArgs(StringID string, StringParameters &args);

/* Do not leak the StringBuilder to everywhere. */
void GenerateTownNameString(StringBuilder &builder, size_t lang, uint32_t seed);
void GetTownName(StringBuilder &builder, const struct Town *t);
void GRFTownNameGenerate(StringBuilder &builder, uint32 grfid, uint16 gen, uint32 seed);

uint RemapNewGRFStringControlCode(uint scc, const char **str, StringParameters &parameters, bool modify_parameters);

#endif /* STRINGS_INTERNAL_H */
