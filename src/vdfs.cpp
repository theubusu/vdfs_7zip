#include "vdfs.h"
#include "util.h"

#include <C/CpuArch.h>

#include <CPP/Common/ComTry.h>
#include <CPP/Common/MyBuffer.h>
#include <CPP/Common/MyCom.h>
#include <CPP/Common/UTFConvert.h>

#include <CPP/7zip/Archive/IArchive.h>
#include <CPP/7zip/Common/LimitedStreams.h>
#include <CPP/7zip/Common/ProgressUtils.h>
#include <CPP/7zip/Common/RegisterArc.h>
#include <CPP/7zip/Common/StreamObjects.h>
#include <CPP/7zip/Common/StreamUtils.h>
#include <CPP/Windows/TimeUtils.h>

#include <CPP/7zip/Compress/CopyCoder.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace Vdfs {
    
    Z7_CLASS_IMP_CHandler_IInArchive_2(IInArchiveGetStream, IOutArchive)
    #if FMTFIX
    {
    #endif
        UInt64 _file_size;
        vdfs4_volume_begins _vb;
        vdfs4_super_block _sb;
        vdfs4_extended_super_block _exsb;
        UInt64 _block_size;
        
        CMyComPtr<IInStream> _inStream;

        using Child = std::pair<std::string, uint64_t>;
        
        std::unordered_map<uint64_t, std::unique_ptr<MyInode>> _inodes;
        std::unordered_map<uint64_t, std::vector<Child>> _cat_tree;

        CObjectVector<InoItem> _items;

        HRESULT Open2(IInStream* stream);
        void run_dir(uint64_t id, const std::string &path);
    };

    // specifies the avaliable properties of the archive itself
    static const Byte kArcProps[] = {
        kpidReadOnly,
    };
    IMP_IInArchive_ArcProps;

    // specifies the avaliable property of the files contained within the archive
    static const Byte kProps[] = {
        kpidPath,
        kpidIsDir,
        kpidSize,
        kpidMTime,      //modification_time
        kpidCTime,      //creation_time
        kpidATime,      //access_time
        kpidGroupId,    //gid
        kpidUserId,     //uid
    };
    IMP_IInArchive_Props;

    // helper used in GetProperty
    static void Utf8StringToProp(const AString& s, NWindows::NCOM::CPropVariant& prop) {
        if (!s.IsEmpty()) {
            UString us;
            ConvertUTF8ToUnicode(s, us);
            prop = us;
        }
    }

    Z7_COM7F_IMF(CHandler::Open(IInStream* stream, const UInt64* /* maxCheckStartPosition */, IArchiveOpenCallback* /* openArchiveCallback */)) {
        DBG_LOG("[vdfs] Open\n");

        COM_TRY_BEGIN
        {
            Close();
            if (Open2(stream) != S_OK) {
                DBG_LOG("[vdfs] Open2 fail\n");
                return S_FALSE;
            }
            DBG_LOG("[vdfs] Open2 ok\n");
            _inStream = stream;
        }
        return S_OK;
        COM_TRY_END
    }

    HRESULT CHandler::Open2(IInStream* stream) {
        InStream_GetSize_SeekToBegin(stream, _file_size);

        //common buf
        Byte w_buf[0x1000];

        //volume begins block    
        RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_volume_begins)));
        memcpy(&_vb, w_buf, sizeof(vdfs4_volume_begins));
        DBG_LOG("[VB] layout ver: %.4s\n", _vb.layout_version);

        //superblock 1 @ 0x200
        RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_super_block)));
        memcpy(&_sb, w_buf, sizeof(vdfs4_super_block));

        DBG_LOG("[SB1] Read only: %i\n", _sb.read_only);
        DBG_LOG("[SB1] Log block size: %i\n", _sb.log_block_size);
        _block_size = 1 << _sb.log_block_size;
        DBG_LOG("[SB1] Block size: %llu\n", _block_size);
        DBG_LOG("[SB1] Max block count: %llu\n", _sb.maximum_blocks_count);
        DBG_LOG("[SB1] Image inode count: %llu\n", _sb.image_inode_count);

        //superblock 2 just will be ignored for now.

        //extended superblock @ 0x600
        RINOK(InStream_SeekSet(stream, 0x600));
        RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_extended_super_block)));
        memcpy(&_exsb, w_buf, sizeof(vdfs4_extended_super_block));

        DBG_LOG("[EXSB] Files count: %llu\n", _exsb.files_count);
        DBG_LOG("[EXSB] Folders count: %llu\n", _exsb.folders_count);

        DBG_LOG("[EXSB] VOLUME BODY - start: %llu\n", _exsb.volume_body.begin);
        DBG_LOG("[EXSB] VOLUME BODY - lenght: %llu\n", _exsb.volume_body.length);

        DBG_LOG("[EXSB] DEBUG AREA - start: %llu\n", _exsb.debug_area.begin);
        DBG_LOG("[EXSB] DEBUG AREA - lenght: %llu\n", _exsb.debug_area.length);

        DBG_LOG("[EXSB] TABLES - start: %llu\n", _exsb.tables.begin);
        DBG_LOG("[EXSB] TABLES - lenght: %llu\n", _exsb.tables.length);

        DBG_LOG("[EXSB] BTREES[0] - start: %llu\n", _exsb.meta[0].begin);
        DBG_LOG("[EXSB] BTREES[0] - lenght: %llu\n", _exsb.meta[0].length);

        UInt64 btrees_offset = _exsb.meta[0].begin * _block_size;
        DBG_LOG("btrees_offset=%llu\n", btrees_offset);
        RINOK(InStream_SeekSet(stream, btrees_offset));


        DBG_LOG("Loading...\n");
        int btree_n = 0;
        while (true) {
            UInt64 pos = 0;
            RINOK(InStream_GetPos(stream, pos));
            if (pos >= _file_size) {
                //EOF
                break;
            }
            UInt64 btables_block = (pos - btrees_offset)/_block_size;
            if (btables_block == _exsb.meta[0].length) {
                DBG_LOG("Reach end of btrees (block %llu)\n", btables_block);
                break;
            }

            char magic[4];
            RINOK(ReadStream_FALSE(stream, magic, 4));
            DBG_LOG("BTREES BLOCK %llu: Magic found: %.4s, Offset: %llu\n", btables_block, magic, pos);

            if (memcmp(magic, NODE_MAGIC, 4) == 0) {    // NODE
                RINOK(InStream_SeekSet(stream, pos));
                RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_gen_node_descr)));
                vdfs4_gen_node_descr nd; 
                memcpy(&nd, w_buf, sizeof(vdfs4_gen_node_descr));
                DBG_LOG("- Bnode %i - type: %i, record count: %i\n", nd.node_id, nd.type, nd.recs_count);

                if (btree_n == 1) { //catalog tree - 1 i know
                    int i;
                    for (i = 0; i < nd.recs_count; i++) {
                        //read node in 2p
                        vdfs4_cattree_key c_key;
                        RINOK(ReadStream_FALSE(stream, w_buf, offsetof(vdfs4_cattree_key, name)));
                        memcpy(&c_key, w_buf, offsetof(vdfs4_cattree_key, name));

                        //read name
                        RINOK(ReadStream_FALSE(stream, w_buf, c_key.name_len));
                        memcpy(c_key.name, w_buf, c_key.name_len);
                        c_key.name[c_key.name_len] = '\0';

                        DBG_LOG("-- KEY %i - Object ID: %llu, Parent ID: %llu, Type: %i, Name: %s (len: %i)\n",
                                i, c_key.object_id, c_key.parent_id, c_key.record_type, c_key.name, c_key.name_len);

                        //all padded to 8 , can be calculated using this. 26 is size of all fields
                        RINOK(ReadStream_FALSE(stream, w_buf, c_key.gen_key.key_len - c_key.name_len - 26));
                        
                        if (nd.type == 1) { //special type for root inode?
                            RINOK(ReadStream_FALSE(stream, w_buf, 4)); // it has a number only idk what for

                        } else if (c_key.record_type == VDFS4_CATALOG_ILINK_RECORD) {
                            //ilink record does not store any extra data
                            /* cattree.c -- parent_id stored as object_id and vice versa !!!*/
                            std::string name(c_key.name);
                            _cat_tree[c_key.object_id].emplace_back(name, c_key.parent_id);

                        } else if (c_key.record_type == VDFS4_CATALOG_HLINK_RECORD) {
                            vdfs4_catalog_hlink_record hlink_record;
                            RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_catalog_hlink_record)));
                            memcpy(&hlink_record, w_buf, sizeof(vdfs4_catalog_hlink_record));
                            DBG_LOG("--- Hlink Record: file_mode: %i", hlink_record.file_mode);

                            //insert hlink into chlidren???
                            std::string name(c_key.name);
                            _cat_tree[c_key.parent_id].emplace_back(name, c_key.object_id);

                        } else if (c_key.record_type == VDFS4_CATALOG_FOLDER_RECORD ) {
                            vdfs4_catalog_folder_record folder_record;
                            RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_catalog_folder_record)));
                            memcpy(&folder_record, w_buf, sizeof(vdfs4_catalog_folder_record));
                            DBG_LOG("--- Folder Record: flags: %i, file_mode: %i, uid: %i, gid: %i, items_count: %llu\n",
                                    folder_record.flags, folder_record.file_mode, folder_record.uid, folder_record.gid, folder_record.total_items_count);

                            MyInode ino;
                            ino.kind = c_key.record_type;
                            ino.folder_record = folder_record;

                            _inodes[c_key.object_id] = std::make_unique<MyInode>(ino);

                        } else if (c_key.record_type == VDFS4_CATALOG_FILE_RECORD) {
                            vdfs4_catalog_file_record file_record;
                            RINOK(ReadStream_FALSE(stream, w_buf, sizeof(vdfs4_catalog_file_record)));
                            memcpy(&file_record, w_buf, sizeof(vdfs4_catalog_file_record));
                            DBG_LOG("--- File: Common Record: flags: %i, file_mode: %i, uid: %i, gid: %i, items_count: %llu\n",
                                    file_record.common.flags, file_record.common.file_mode, file_record.common.uid, file_record.common.gid, file_record.common.total_items_count);
                            DBG_LOG("--- File Record: Size in bytes: %llu, Total block count: %llu\n",
                                    file_record.data_fork.size_in_bytes, file_record.data_fork.total_blocks_count);

                            MyInode ino;
                            ino.kind = c_key.record_type;
                            ino.file_record = file_record;

                            _inodes[c_key.object_id] = std::make_unique<MyInode>(ino);
                        }
                    }

                    UInt64 new_pos = 0;
                    RINOK(InStream_GetPos(stream, new_pos));
                    UInt64 diff_pos = new_pos - pos;

                    RINOK(InStream_SeekSet(stream, new_pos + (4*_block_size) - diff_pos /* read data*/));

                } else {
                    RINOK(InStream_SeekSet(stream, pos + (4*_block_size))); // 4 blocks
                }


            } else if (memcmp(magic, VDFS4_BTREE_HEAD_NODE_MAGIC, 4) == 0) {
                //btree skip 4 blocks
                RINOK(InStream_SeekSet(stream, pos + (4*_block_size))); // 4 blocks
                btree_n += 1;

            } else if (memcmp(magic, BTREES_END_MAGIC, 4) == 0) {
                DBG_LOG("BTREES BLOCK %llu, Offset: %llu - Premature end of btrees blocks??\n", btables_block, pos);
                break;

            } else {
                //non important/unknown node (fsmb, inob...)
                RINOK(InStream_SeekSet(stream, pos + (1*_block_size))); // 1 block
            }
        }

        std::string path_str; 
        run_dir(1, path_str);

        
        /*
        int i;
        for (i = 0; i < _items.Size(); i++) {
            InoItem item = _items[i];
            DBG_LOG("%s\n", item.path.c_str());
        }
        */


        return S_OK;
    }

    void CHandler::run_dir(uint64_t id, const std::string &path) {
        auto &inode = _inodes.at(id);
        auto it = _cat_tree.find(id);
        if (it == _cat_tree.end()) {
            return;
        }

        for (auto &child : it->second) {
            const std::string &name = child.first;
            uint64_t child_id = child.second;
            std::string new_path = path.empty() ? name : path + "/" + name;

            //DBG_LOG("%s\n", new_path.c_str());

            auto &child_inode = *_inodes.at(child_id);
            InoItem item;
            item.path = new_path;
            item.inode_id = child_id;
            _items.Add(item);

            if (child_inode.kind == VDFS4_CATALOG_FOLDER_RECORD) {
                run_dir(child_id, new_path);
            }
        }
    }

    Z7_COM7F_IMF(CHandler::Close()) {

        _inStream.Release();
        _items.Clear();
        return S_OK;
    }

    Z7_COM7F_IMF(CHandler::Extract(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback* extractCallback)) {

        COM_TRY_BEGIN

        const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
        if (allFilesMode) {
            numItems = _items.Size();
        }

        if (numItems == 0) {
            return S_OK;
        }

        UInt64 total_size = 0;
        for (UInt32 i = 0; i < numItems; i++) {
            const InoItem& item = _items[allFilesMode ? i : indices[i]];
            const MyInode& inode = *_inodes.at(item.inode_id);

            if (!(inode.kind == VDFS4_CATALOG_FILE_RECORD)) {
                continue;
            }

            const auto& fr = inode.file_record;
            total_size += fr.data_fork.size_in_bytes;
        }

        extractCallback->SetTotal(total_size);
        UInt64 currentTotal = 0;

        for (UInt32 i = 0; i < numItems; i++) {
            const UInt32 index = allFilesMode ? i : indices[i];
            const InoItem& item = _items[index];
            const MyInode& inode = *_inodes.at(item.inode_id);

            const bool is_dir = (inode.kind == VDFS4_CATALOG_FOLDER_RECORD);

            CMyComPtr<ISequentialOutStream> outStream;
            const Int32 askMode = testMode
                ? NArchive::NExtract::NAskMode::kTest
                : NArchive::NExtract::NAskMode::kExtract;

            RINOK(extractCallback->GetStream(index, &outStream, askMode));
            RINOK(extractCallback->PrepareOperation(askMode));

            //no real test now
            if (!testMode && !outStream) {
                continue;
            }
            if (testMode) {
                extractCallback->SetOperationResult(NArchive::NExtract::NOperationResult::kOK);
                continue;
            }

            UInt64 written_total = 0;
            bool ok = true;

            if (is_dir) {
                extractCallback->SetOperationResult(NArchive::NExtract::NOperationResult::kOK);
                continue;
            }

            const auto& fr = inode.file_record;
            const UInt64 file_size = fr.data_fork.size_in_bytes;
            UInt64 remain = file_size;

            // INLINE FILE
            if (fr.common.flags & VDFS4_INLINE_DATA_FILE){
                const Byte* data = fr.data_fork.inline_data;
                RINOK(WriteStream(outStream, data, (size_t)file_size));
                written_total = file_size;

            } else {
                for (const auto& ext : fr.data_fork.extents) {
                    if (ext.extent.length == 0) {
                        break;
                    }

                    UInt64 offset = ext.extent.begin * _block_size;
                    UInt64 to_read = ext.extent.length * _block_size;
                    if (to_read > remain) {
                        to_read = remain;
                    }
                    if (to_read == 0) {
                        break;
                    }

                    RINOK(InStream_SeekSet(_inStream, offset));
                    std::vector<Byte> buf((size_t)to_read);

                    RINOK(ReadStream_FALSE(_inStream, buf.data(), (UInt32)to_read));

                    UInt32 written = 0;
                    RINOK(outStream->Write(buf.data(), buf.size(), &written));

                    written_total += written;
                    if (written != buf.size()) {
                        ok = false;
                    }
                    remain -= written;
                }

            }
            currentTotal += file_size;
            RINOK(extractCallback->SetOperationResult(
                ok ? NArchive::NExtract::NOperationResult::kOK
                    : NArchive::NExtract::NOperationResult::kDataError));
        }

        return S_OK;
        COM_TRY_END
    }
    

    Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32* numItems)) {

        *numItems = _items.Size();
        return S_OK;
    }

    Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT* value)) {

        COM_TRY_BEGIN
        NWindows::NCOM::CPropVariant prop;
        switch (propID) {
            case kpidExtension:
                prop = "vdfs";
                break;
        }
        prop.Detach(value);
        return S_OK;
        COM_TRY_END
    }

    Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value)) {

        COM_TRY_BEGIN
        NWindows::NCOM::CPropVariant prop;
        const InoItem& item = _items[index];
        const MyInode& inode = *_inodes.at(item.inode_id);
        AString itemPath(item.path.c_str());
        const auto& common =
            (inode.kind == VDFS4_CATALOG_FILE_RECORD)
            ? inode.file_record.common
            : inode.folder_record;

        switch (propID) {
            case kpidPath:  
                Utf8StringToProp(itemPath, prop);
                break;
            case kpidIsDir:
                prop = (inode.kind == VDFS4_CATALOG_FOLDER_RECORD);
                break;
            case kpidSize:
                prop = (inode.kind == VDFS4_CATALOG_FILE_RECORD)
                   ? inode.file_record.data_fork.size_in_bytes
                   : 0; 
                break;
            case kpidGroupId:
                prop = common.gid;
                break;
            case kpidUserId:
                prop = common.uid;
                break;
            case kpidMTime:
                PropVariant_SetFrom_UnixTime(prop, common.modification_time.seconds);
                break;
            case kpidCTime:
                PropVariant_SetFrom_UnixTime(prop, common.creation_time.seconds);
                break;
            case kpidATime:
                PropVariant_SetFrom_UnixTime(prop, common.access_time.seconds);
                break;

        }
        prop.Detach(value);
        return S_OK;
        COM_TRY_END
    }
 
    Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream** stream)) {

        //no need, if there is no stream it will just use extract function
        return S_OK;
    }
    
    Z7_COM7F_IMF(CHandler::GetFileTimeType(UInt32* type)) {

        *type = k_PropVar_TimePrec_Unix;
        return S_OK;
    }

    Z7_COM7F_IMF(CHandler::UpdateItems(ISequentialOutStream* outStream, UInt32 numItems, IArchiveUpdateCallback* callback )) {

        COM_TRY_BEGIN
        return S_OK;
        COM_TRY_END
    }

    //register format
    REGISTER_ARC_I(
        "vdfs",                 // format name
        "vdfs",                  // file extension
        NULL,                   // ?ae
        0xA2,                   // unique id for GUID
        vdfs_signature,         // file magic signature
        0,                      // offset of signature
        0,                      // arc flags
        0                       // isArc
    )
}