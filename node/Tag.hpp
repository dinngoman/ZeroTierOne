/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZT_TAG_HPP
#define ZT_TAG_HPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "C25519.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A tag that can be associated with members and matched in rules
 *
 * Capabilities group rules, while tags group members subject to those
 * rules. Tag values can be matched in rules, and tags relevant to a
 * capability are presented along with it.
 *
 * E.g. a capability might be "can speak Samba/CIFS within your
 * department." This cap might have a rule to allow TCP/137 but
 * only if a given tag ID's value matches between two peers. The
 * capability is what members can do, while the tag is who they are.
 * Different departments might have tags with the same ID but different
 * values.
 *
 * Unlike capabilities tags are signed only by the issuer and are never
 * transferrable.
 */
class Tag
{
public:
	Tag()
	{
		memset(this,0,sizeof(Tag));
	}

	/**
	 * @param nwid Network ID
	 * @param expiration Tag expiration relative to network config timestamp
	 * @param issuedTo Address to which this tag was issued
	 * @param id Tag ID
	 * @param value Tag value
	 */
	Tag(const uint64_t nwid,const uint64_t expiration,const Address &issuedTo,const uint32_t id,const uint32_t value) :
		_nwid(nwid),
		_expiration(expiration),
		_id(id),
		_value(value),
		_issuedTo(issuedTo),
		_signedBy()
	{
	}


	inline uint64_t networkId() const { return _nwid; }
	inline uint64_t expiration() const { return _expiration; }
	inline uint32_t id() const { return _id; }
	inline uint32_t value() const { return _value; }
	inline const Address &issuedTo() const { return _issuedTo; }
	inline const Address &signedBy() const { return _signedBy; }

	/**
	 * Sign this tag
	 *
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	inline bool sign(const Identity &signer)
	{
		try {
			Buffer<(sizeof(Tag) * 2)> tmp;
			_signedBy = signer.address();
			this->serialize(tmp,true);
			_signature = signer.sign(tmp.data(),tmp.size());
			return true;
		} catch ( ... ) {}
		return false;
	}

	/**
	 * Check this tag's signature
	 *
	 * @param RR Runtime environment to allow identity lookup for signedBy
	 * @return True if signature is present and valid
	 */
	bool verify(const RuntimeEnvironment *RR);

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append(_nwid);
		b.append(_expiration);
		b.append(_id);
		b.append(_value);
		_issuedTo.appendTo(b);
		_signedBy.appendTo(b);
		if (!forSign) {
			b.append((uint8_t)1); // 1 == Ed25519
			b.append((uint16_t)ZT_C25519_SIGNATURE_LEN); // length of signature
			b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		}
		b.append((uint16_t)0); // length of additional fields, currently 0

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		_nwid = b.template at<uint64_t>(p); p += 8;
		_expiration = b.template at<uint64_t>(p); p += 8;
		_id = b.template at<uint32_t>(p); p += 4;
		_value = b.template at<uint32_t>(p); p += 4;
		_issuedTo.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_signedBy.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		if (b[p++] != 1)
			throw std::runtime_error("unrecognized signature type");
		if (b.template at<uint16_t>(p) != ZT_C25519_SIGNATURE_LEN)
			throw std::runtime_error("invalid signature length");
		p += 2;
		memcpy(_signature.data,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN); p += ZT_C25519_SIGNATURE_LEN;

		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw std::runtime_error("extended field overflow");

		return (p - startAt);
	}

private:
	uint64_t _nwid;
	uint64_t _expiration;
	uint32_t _id;
	uint32_t _value;
	Address _issuedTo;
	Address _signedBy;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
