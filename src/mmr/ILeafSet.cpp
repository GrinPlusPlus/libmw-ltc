#include <mw/mmr/LeafSet.h>
#include <mw/crypto/Hasher.h>

MMR_NAMESPACE

void ILeafSet::Add(const LeafIndex& idx)
{
	uint8_t byte = GetByte(idx.Get() / 8);
	byte |= BitToByte(idx.Get() % 8);
	SetByte(idx.Get() / 8, byte);

	if (idx >= m_nextLeafIdx) {
		m_nextLeafIdx = idx.Next();
	}
}

void ILeafSet::Remove(const LeafIndex& idx)
{
	uint8_t byte = GetByte(idx.GetLeafIndex() / 8);
	byte &= (0xff ^ BitToByte(idx.GetLeafIndex() % 8));
	SetByte(idx.GetLeafIndex() / 8, byte);
}

bool ILeafSet::Contains(const LeafIndex& idx) const noexcept
{
	return GetByte(idx.GetLeafIndex() / 8) & BitToByte(idx.GetLeafIndex() % 8);
}

mw::Hash ILeafSet::Root() const
{
	//uint8_t bitsToClear = 0;
	uint64_t numBytes = (m_nextLeafIdx.GetLeafIndex() + 7) / 8;
	//if (numLeaves % 8 != 0) {
	//	++numBytes;
	//	//bitsToClear = 8 - (numLeaves % 8);
	//}

	std::vector<uint8_t> bytes(numBytes);
	for (uint64_t byte_idx = 0; byte_idx < numBytes; byte_idx++) {
		uint8_t byte = GetByte(byte_idx);
		//if (byte_idx == (numBytes - 1)) {
		//	for (uint8_t bit = 0; bit < bitsToClear; bit++) {
		//		byte &= ~(1 << bit);
		//	}
		//}
		bytes[byte_idx] = byte;
	}

	return Hashed(bytes);
}

void ILeafSet::Rewind(const uint64_t numLeaves, const std::vector<LeafIndex>& leavesToAdd)
{
	for (const LeafIndex& idx : leavesToAdd) {
		Add(idx);
	}

	for (size_t i = numLeaves; i < m_nextLeafIdx.GetLeafIndex(); i++) {
		Remove(LeafIndex::At(i));
	}

	m_nextLeafIdx = mmr::LeafIndex::At(numLeaves);
}

// Returns a byte with the given bit (0-7) set.
// Example: BitToByte(2) returns 32 (00100000).
uint8_t ILeafSet::BitToByte(const uint8_t bit) const
{
	return 1 << (7 - bit);
}

END_NAMESPACE