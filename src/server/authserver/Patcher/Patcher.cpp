#include "AuthSession.h"
#include "Patcher.h"
#include "PatchMgr.h"
#include <fstream>

#define CHUNK_SIZE 4096

Patcher::Patcher(AuthSession* Session, PatchMgr::PatchInfo* Patch) : _stopped(false), _session(Session), _patch(Patch) { }
Patcher::~Patcher()
{
    _session->_patcher = nullptr;
    _patch = nullptr;
}

void Patcher::Init(uint64 start_pos)
{
    if (!_patch)
        return;

    std::ifstream file(_patch->Path, std::ios::binary);
    if (!file.is_open())
        return;

    file.seekg(start_pos);
    while (!_stopped && (uint64)file.tellg() < _patch->Size)
    {
        uint64 remaining = _patch->Size - file.tellg();
        uint16 chunkSize = (remaining >= CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        ByteBuffer pkt;
        char* bytes = new char[chunkSize];
        TransferDataPacket hdr;
        hdr.cmd = XFER_DATA;
        hdr.chunk_size = chunkSize;
        file.read(bytes, chunkSize);
        pkt.append(&hdr, 1);
        pkt.append(bytes, chunkSize);

        _session->SendPacket(pkt);
        delete[] bytes;

        // we need it?
        //Sleep(100);
    }

    file.close();
    delete this;
}

void Patcher::Stop()
{
    _stopped = true;
}
