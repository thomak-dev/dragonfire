#include "pch.h"

#include "ResourceManager.h"

void ResourceManager::Update()
{
    if (freeRequests.size() > 0)
    {
        waitFunction();
        for (auto& freeThis : freeRequests)
        {
            FreeInternal(freeThis);
        }
        freeRequests.clear();
    }
}

void ResourceManager::FreeInternal(const std::string& key)
{
    try
    {
        auto& res = resources.at(key);
        if (res.ptr.use_count() == 1)
            resources.erase(key);
    }
    catch (std::out_of_range&)
    {
    }
}