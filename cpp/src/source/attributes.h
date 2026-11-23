// An IMFAttributes implementation that forwards every call to a real attribute store.
// Media Foundation expects the activation object, the media source and the media stream to
// *be* attribute stores, so each of them derives from this.
#pragma once

#include <mfapi.h>
#include <mfobjects.h>
#include <wrl/client.h>

namespace tumon {

template <class Base>
class AttributesBase : public Base {
public:
    STDMETHODIMP GetItem(REFGUID key, PROPVARIANT* value) override { return m_attrs->GetItem(key, value); }
    STDMETHODIMP GetItemType(REFGUID key, MF_ATTRIBUTE_TYPE* type) override { return m_attrs->GetItemType(key, type); }
    STDMETHODIMP CompareItem(REFGUID key, REFPROPVARIANT value, BOOL* result) override { return m_attrs->CompareItem(key, value, result); }
    STDMETHODIMP Compare(IMFAttributes* theirs, MF_ATTRIBUTES_MATCH_TYPE type, BOOL* result) override { return m_attrs->Compare(theirs, type, result); }
    STDMETHODIMP GetUINT32(REFGUID key, UINT32* value) override { return m_attrs->GetUINT32(key, value); }
    STDMETHODIMP GetUINT64(REFGUID key, UINT64* value) override { return m_attrs->GetUINT64(key, value); }
    STDMETHODIMP GetDouble(REFGUID key, double* value) override { return m_attrs->GetDouble(key, value); }
    STDMETHODIMP GetGUID(REFGUID key, GUID* value) override { return m_attrs->GetGUID(key, value); }
    STDMETHODIMP GetStringLength(REFGUID key, UINT32* length) override { return m_attrs->GetStringLength(key, length); }
    STDMETHODIMP GetString(REFGUID key, LPWSTR buffer, UINT32 size, UINT32* length) override { return m_attrs->GetString(key, buffer, size, length); }
    STDMETHODIMP GetAllocatedString(REFGUID key, LPWSTR* value, UINT32* length) override { return m_attrs->GetAllocatedString(key, value, length); }
    STDMETHODIMP GetBlobSize(REFGUID key, UINT32* size) override { return m_attrs->GetBlobSize(key, size); }
    STDMETHODIMP GetBlob(REFGUID key, UINT8* buffer, UINT32 size, UINT32* blobSize) override { return m_attrs->GetBlob(key, buffer, size, blobSize); }
    STDMETHODIMP GetAllocatedBlob(REFGUID key, UINT8** buffer, UINT32* size) override { return m_attrs->GetAllocatedBlob(key, buffer, size); }
    STDMETHODIMP GetUnknown(REFGUID key, REFIID riid, LPVOID* object) override { return m_attrs->GetUnknown(key, riid, object); }
    STDMETHODIMP SetItem(REFGUID key, REFPROPVARIANT value) override { return m_attrs->SetItem(key, value); }
    STDMETHODIMP DeleteItem(REFGUID key) override { return m_attrs->DeleteItem(key); }
    STDMETHODIMP DeleteAllItems() override { return m_attrs->DeleteAllItems(); }
    STDMETHODIMP SetUINT32(REFGUID key, UINT32 value) override { return m_attrs->SetUINT32(key, value); }
    STDMETHODIMP SetUINT64(REFGUID key, UINT64 value) override { return m_attrs->SetUINT64(key, value); }
    STDMETHODIMP SetDouble(REFGUID key, double value) override { return m_attrs->SetDouble(key, value); }
    STDMETHODIMP SetGUID(REFGUID key, REFGUID value) override { return m_attrs->SetGUID(key, value); }
    STDMETHODIMP SetString(REFGUID key, LPCWSTR value) override { return m_attrs->SetString(key, value); }
    STDMETHODIMP SetBlob(REFGUID key, const UINT8* buffer, UINT32 size) override { return m_attrs->SetBlob(key, buffer, size); }
    STDMETHODIMP SetUnknown(REFGUID key, IUnknown* unknown) override { return m_attrs->SetUnknown(key, unknown); }
    STDMETHODIMP LockStore() override { return m_attrs->LockStore(); }
    STDMETHODIMP UnlockStore() override { return m_attrs->UnlockStore(); }
    STDMETHODIMP GetCount(UINT32* count) override { return m_attrs->GetCount(count); }
    STDMETHODIMP GetItemByIndex(UINT32 index, GUID* key, PROPVARIANT* value) override { return m_attrs->GetItemByIndex(index, key, value); }
    STDMETHODIMP CopyAllItems(IMFAttributes* dest) override { return m_attrs->CopyAllItems(dest); }

protected:
    HRESULT InitAttributes(UINT32 initialSize = 8) { return MFCreateAttributes(&m_attrs, initialSize); }
    void ReleaseAttributes() { m_attrs.Reset(); }

    Microsoft::WRL::ComPtr<IMFAttributes> m_attrs;
};

}  // namespace tumon
