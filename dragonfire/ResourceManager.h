#pragma once

#include "tmp.h"

class ResourceManager
{
public:
    template <typename T, typename Loader, typename Destroyer, typename = std::enable_if_t<!std::is_pointer<T>::value>>
    void SetLoaderAndDestroyer(Loader loader, Destroyer destroyer);

    template <typename T, typename = std::enable_if_t<!std::is_pointer<T>::value && !has_ctype_member<T>::value>>
    std::shared_ptr<T> Get(std::string_view path) noexcept;

    template <typename T>
    std::shared_ptr<typename std::remove_pointer<typename T::CType>::type> Get(std::string_view path) noexcept
    {
        return Get<typename std::remove_pointer<typename T::CType>::type>(path);
    }
    void Clear() noexcept { resources.clear(); }

private:
    using LoaderFunction = std::function<void*(std::string_view)>;
    using DestroyerFunction = std::function<void(void*)>;

    struct Resource
    {
        uint32_t type{0xFFFFFFFF};
        std::shared_ptr<void> ptr;
    };

    struct FunctionPair
    {
        LoaderFunction loader;
        DestroyerFunction destroyer;
    };

    std::unordered_map<std::string, Resource> resources;
    std::vector<FunctionPair> functions;

    static uint32_t GetNextTypeId() noexcept
    {
        static uint32_t id = 0;
        return id++;
    }

    template <typename T, typename = std::enable_if_t<!has_ctype_member<T>::value>>
    static uint32_t GetTypeId() noexcept
    {
        static uint32_t id = GetNextTypeId();
        return id;
    }

    template <typename T>
    static uint32_t GetTypeId(typename std::enable_if<has_ctype_member<T>::value>::type* = nullptr) noexcept
    {
        return GetTypeId<typename std::remove_pointer<typename T::CType>::type>();
    }
};

template <typename T, typename>
std::shared_ptr<T> ResourceManager::Get(std::string_view path) noexcept
{
    try
    {
        auto element = resources.at(std::string{path});
        if (element.type != GetTypeId<T>())
            return nullptr;
        return std::reinterpret_pointer_cast<T>(element.ptr);
    }
    catch (std::out_of_range&)
    {
        const auto type = GetTypeId<T>();
        if (type >= functions.size())
            return nullptr;

        try
        {
            auto resource = functions[type].loader(path);
            auto ptr = std::shared_ptr<void>{resource, functions[type].destroyer};
            resources[std::string{path}] = {type, ptr};
            return std::reinterpret_pointer_cast<T>(ptr);
        }
        catch (...)
        {
            return nullptr;
        }
    }
}

template <typename T, typename Loader, typename Destroyer, typename>
void ResourceManager::SetLoaderAndDestroyer(Loader loader, Destroyer destroyer)
{
    static_assert(noexcept(destroyer(nullptr)), "Destroyer must be noexcept.");
    const size_t index = GetTypeId<T>();
    if (index >= functions.size())
        functions.resize(index + 1);
    functions[index].loader = loader;
    functions[index].destroyer = destroyer;
}
