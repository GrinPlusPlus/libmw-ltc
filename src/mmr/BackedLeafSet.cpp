#include <mw/mmr/LeafSet.h>
#include <mw/crypto/Hasher.h>

MMR_NAMESPACE

void BackedLeafSet::Add(const LeafIndex& idx)
{
	uint8_t byte = GetByte(idx.GetLeafIndex() / 8);
	byte |= BitToByte(idx.GetLeafIndex() % 8);
	m_modifiedBytes[idx.GetLeafIndex() / 8] = byte;
}

void BackedLeafSet::Remove(const LeafIndex& idx)
{
	uint8_t byte = GetByte(idx.GetLeafIndex() / 8);
	byte &= (0xff ^ BitToByte(idx.GetLeafIndex() % 8));
	m_modifiedBytes[idx.GetLeafIndex() / 8] = byte;
}

bool BackedLeafSet::Contains(const LeafIndex& idx) const noexcept
{
	return GetByte(idx.GetLeafIndex() / 8) & BitToByte(idx.GetLeafIndex() % 8);
}

mw::Hash BackedLeafSet::Root(const uint64_t numLeaves) const
{
	uint8_t bitsToClear = 0;
	uint64_t numBytes = (numLeaves / 8);
	if (numLeaves % 8 != 0) {
		++numBytes;
		bitsToClear = 8 - (numLeaves % 8);
	}

	std::vector<uint8_t> bytes(numBytes);
	for (uint64_t byte_idx = 0; byte_idx < numBytes; byte_idx++)
	{
		uint8_t byte = GetByte(byte_idx);
		if (byte_idx == (numBytes - 1)) {
			for (uint8_t bit = 0; bit < bitsToClear; bit++)
			{
				byte &= ~(1 << bit);
			}
		}
		bytes[byte_idx] = byte;
	}

	return Hashed(bytes);
}

uint64_t BackedLeafSet::GetSize() const
{
	size_t size = m_pBacked->GetSize();
	for (auto iter = m_modifiedBytes.cbegin(); iter != m_modifiedBytes.cend(); iter++)
	{
		if (iter->first >= size) {
			size = iter->first + 1;
		}
	}

	return size * 8;
}

void BackedLeafSet::Rewind(const uint64_t numLeaves, const std::vector<LeafIndex>& leavesToAdd)
{
	for (const LeafIndex& idx : leavesToAdd)
	{
		Add(idx);
	}

	size_t currentSize = GetSize();
	for (size_t i = numLeaves; i < currentSize; i++)
	{
		Remove(LeafIndex::At(i));
	}
}

uint8_t BackedLeafSet::GetByte(const uint64_t byteIdx) const
{
	auto iter = m_modifiedBytes.find(byteIdx);
	if (iter != m_modifiedBytes.cend())
	{
		return iter->second;
	}

	return m_pBacked->GetByte(byteIdx);
}

// Returns a byte with the given bit (0-7) set.
// Example: BitToByte(2) returns 32 (00100000).
uint8_t BackedLeafSet::BitToByte(const uint8_t bit) const
{
	return 1 << (7 - bit);
}

END_NAMESPACE