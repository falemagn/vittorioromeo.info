#include <vector>
#include <map>
#include <string>
#include <experimental/optional>
#include <SSVUtils/Core/Core.hpp>
#include <SSVUtils/Json/Json.hpp>
#include <SSVUtils/TemplateSystem/TemplateSystem.hpp>
#include <DiscountCpp/DiscountCpp.hpp>

namespace std
{
    using namespace std::experimental;
}

using sz_t = std::size_t;
using page_id = sz_t;
using entry_id = sz_t;

namespace utils
{
    auto path_depth(const ssvufs::Path& p)
    {
        return ssvu::getCharCount(p, '/');
    }

    auto resources_folder_path(sz_t depth)
    {
        ssvufs::Path result;

        for(sz_t i(0); i < depth + 1; ++i)
        {
            result += "../";
        }

        return result + "resources";
    }

    auto html_from_md(const ssvufs::Path& p)
    {
        return discountcpp::getHTMLFromMarkdownFile(p);
    }

    auto expand_to_dictionary(
        const ssvufs::Path& working_directory, const ssvj::Val& mVal)
    {
        ssvu::TemplateSystem::Dictionary result;

        for(const auto& p : mVal.forObjAs<std::string>())
        {
            if(ssvu::endsWith(p.value, ".md"))
            {
                result[p.key] =
                    html_from_md(working_directory + ssvufs::Path{p.value});
            }
            else
            {
                result[p.key] = p.value;
            }
        }

        return result;
    }

    void write_to_file(ssvufs::Path p, const std::string& s)
    {
        auto p_parent = p.getParent();
        if(!p_parent.exists<ssvufs::Type::Folder>())
        {
            std::string cmd("mkdir -p "s + p_parent.getStr());
            system(cmd.c_str());
        }

        assert(p_parent.exists<ssvufs::Type::Folder>());

        std::ofstream o{p};
        o << s;
        o.flush();
        o.close();
    }

    template <typename TF>
    void segmented_for(sz_t split_count, sz_t per_split, sz_t total, TF&& f)
    {
        assert(split_count > 0);

        for(sz_t i_split = 0; i_split < split_count - 1; ++i_split)
        {
            auto i_begin = i_split * per_split;
            auto i_end = (i_split + 1) * per_split;
            f(i_split, i_begin, i_end);
        }

        {
            auto i_split = split_count - 1;
            auto i_begin = i_split * per_split;
            auto i_end = total;
            f(i_split, i_begin, i_end);
        }
    }
}



namespace archetype
{
    struct entry
    {
        ssvufs::Path _template_path;
        ssvu::TemplateSystem::Dictionary _expand;
        ssvufs::Path _output_path;
        page_id _parent_page;
        std::optional<std::string> _link_name;
    };

    struct page
    {
        std::string _name;
        ssvufs::Path _path;
        std::string _full_name;
        ssvufs::Path _output_path;
        std::optional<sz_t> _subpaging;
        std::vector<entry_id> _entries;
    };
}

namespace structure
{
    class entry_mapping
    {
    private:
        std::map<entry_id, archetype::entry> _archetypes;

    public:
        auto& add(entry_id eid)
        {
            _archetypes[eid] = archetype::entry{};
            return _archetypes[eid];
        }

        auto get(entry_id eid) const
        {
            return _archetypes.at(eid);
        }

        template <typename TF>
        void for_all(TF&& f)
        {
            for(auto& a : _archetypes)
            {
                f(std::get<0>(a), std::get<1>(a));
            }
        }
    };

    class page_mapping
    {
    private:
        std::map<page_id, archetype::page> _archetypes;

    public:
        auto& add(page_id pid)
        {
            _archetypes[pid] = archetype::page{};
            return _archetypes[pid];
        }

        auto get(page_id pid) const
        {
            return _archetypes.at(pid);
        }

        template <typename TF>
        void for_all(TF&& f)
        {
            for(auto& a : _archetypes)
            {
                f(std::get<0>(a), std::get<1>(a));
            }
        }
    };

    /*
    class page_hierarchy
    {
    private:
        std::map<page_id, std::vector<page_id>> _children;
        std::map<page_id, page_id> _parents;

    public:
        void add(page_id parent, page_id child)
        {
            _children[parent].emplace_back(child);
            _parents[child] = parent;
        }
    };
    */
}



constexpr const char* reserved_fld_pages = "_pages";
constexpr const char* reserved_fld_entries = "_entries";
constexpr const char* reserved_fld_asides = "_asides";

constexpr const char* pages_path = "content/_pages/";
constexpr const char* result_path = "result/";
constexpr const char* page_json = "_page.json";


using namespace ssvu::FileSystem;
using namespace ssvu::TemplateSystem;
using namespace ssvu::Json;

template <typename TF>
void for_all_page_json_files(TF&& f)
{
    std::vector<Path> page_json_paths{
        getScan<Mode::Recurse, Type::File, Pick::ByName>(
            pages_path, page_json)};

    for(auto& p : page_json_paths)
    {
        const auto& path = p;
        const auto& name = p.getParent().getFolderName();

        const auto& full_name = ssvu::getTrimR(
            ssvu::getReplaced(p.getParent().getStr(), pages_path, ""),
            [](char c)
            {
                return c == '/';
            });

        const auto& contents = ssvj::fromFile(path);

        f(path, name, full_name, contents);
    }
}

template <typename TF>
void for_all_entry_json_files(ssvufs::Path page_path, TF&& f)
{
    ssvufs::Path entries_path = page_path.getParent() + "_entries/";
    if(!entries_path.exists<ssvufs::Type::Folder>())
    {
        return;
    }

    std::vector<Path> entry_json_paths{
        getScan<Mode::Recurse, Type::File, Pick::ByExt>(pages_path, ".json")};

    for(auto& p : entry_json_paths)
    {
        const auto& path = p;
        const auto& name = ssvu::getReplaced(p.getFileName(), ".json", "");

        if(name == "_page")
        {
            continue;
        }

        const auto& full_name = ssvu::getReplaced(
            ssvu::getTrimR(ssvu::getReplaced(p.getStr(), entries_path, ""),
                [](char c)
                {
                    return c == '/';
                }),
            ".json", "");

        const auto& contents = ssvj::fromFile(path);

        f(path, name, full_name, contents);
    }
}

struct context
{
    page_id _next_page_id = 0;
    entry_id _next_entry_id = 0;

    structure::entry_mapping _entry_mapping;
    structure::page_mapping _page_mapping;

    // structure::page_hierarchy _page_hierarchy;
};

struct subpage_expansion
{
    std::vector<std::string> _expanded_entries;

    auto produce_result(const Path& output_path,
        const std::vector<std::string>& expanded_asides)
    {
        Dictionary dict;

        for(const auto& e : _expanded_entries)
        {
            dict["Entries"] += Dictionary{"Entry", e};
        }

        for(const auto& a : expanded_asides)
        {
            dict["Asides"] += Dictionary{"Aside", a};
        }

        auto main_exp =
            dict.getExpanded(Path{"templates/base/main.tpl"}.getContentsAsStr(),
                Settings::MaintainUnexisting);

        Path resourcesPath{
            utils::resources_folder_path(utils::path_depth(output_path) - 1)};

        Dictionary dict2;
        // dict["MainMenu"] = mainMenu.getOutput();
        dict["Main"] = main_exp;
        dict["ResourcesPath"] = resourcesPath;
        return dict.getExpanded(Path{"templates/page.tpl"}.getContentsAsStr(),
            Settings::MaintainUnexisting);
    }
};

struct page_expansion
{
    std::vector<std::string> _expanded_asides;
    std::vector<subpage_expansion> _subpages;

    auto produce_result(const Path& output_path)
    {
        assert(_subpages.size() > 0);
        auto& first_subpage = _subpages[0];

        auto first_subpage_html =
            first_subpage.produce_result(output_path, _expanded_asides);

        utils::write_to_file(output_path, first_subpage_html);

        for(sz_t i = 1; i < _subpages.size(); ++i)
        {
            auto adapted_op = output_path.getStr();
            ssvu::replace(
                adapted_op, ".html", "/" + std::to_string(i) + ".html");

            utils::write_to_file(
                adapted_op, _subpages[i].produce_result(
                                Path{adapted_op}, _expanded_asides));
        }
    }
};


int main()
{
    Path rp{result_path};
    if(rp.exists<Type::Folder>())
    {
        std::string cmd("rm -R "s + result_path);
        system(cmd.c_str());
    }

    if(!rp.exists<Type::Folder>())
    {
        ssvufs::createFolder(rp);
    }

    context ctx;

    for_all_page_json_files(
        [&ctx](auto path, auto name, auto full_name, Val contents)
        {
            // Alias next page id.
            auto& pid = ctx._next_page_id;

            // Log data about page.
            ssvu::lo("Page|id") << pid << "\n";
            ssvu::lo("Page|path") << path << "\n";
            ssvu::lo("Page|name") << name << "\n";
            ssvu::lo("Page|full_name") << full_name << "\n";

            auto output_path = std::string{result_path} + full_name + ".html";
            ssvu::lo("Page|output_path") << output_path << "\n";

            // Register page.
            auto& ap = ctx._page_mapping.add(pid);
            ap._name = name;
            ap._path = path;
            ap._full_name = full_name;
            ap._output_path = output_path;

            // Check for subpaging options.
            if(contents.has("subpaging"))
            {
                auto eps =
                    contents["subpaging"]["entries_per_subpage"].as<IntU>();
                ap._subpaging = eps;
                ssvu::lo("Page|eps") << eps << "\n";
            }

            // Get entries.
            for_all_entry_json_files(path,
                [&ctx, &output_path, &pid, &ap](auto e_path, auto e_name,
                                         auto e_full_name, Val es_contents)
                {
                    auto& eid = ctx._next_entry_id;

                    for(Val e_contents : es_contents.forArr())
                    {
                        ssvu::lo("Entry|parent_page") << pid << "\n";
                        ssvu::lo("Entry|path") << e_path << " [" << eid
                                               << "]\n";
                        ssvu::lo("Entry|name") << e_name << "/" << eid << "\n";
                        ssvu::lo("Entry|full_name") << e_full_name << "/" << eid
                                                    << "\n";

                        auto e_template_path = e_contents["template"].as<Str>();
                        auto e_expand_data = e_contents["expand"].as<Val>();
                        auto wd = e_path.getParent();
                        auto dic =
                            utils::expand_to_dictionary(wd, e_expand_data);

                        auto e_output_path =
                            Path{ssvu::getReplaced(output_path, ".html", "")} +
                            "/" + e_full_name;

                        // Register entry.
                        ap._entries.emplace_back(eid);
                        auto& ae = ctx._entry_mapping.add(eid);

                        if(e_contents.has("link_name"))
                        {
                            auto link_name = e_contents["link_name"].as<Str>();
                            ae._link_name = link_name;
                            e_output_path += "/" + link_name + ".html";
                        }
                        else
                        {
                            e_output_path +=
                                "/" + std::to_string(eid) + ".html";
                        }



                        ae._template_path = e_template_path;
                        ae._expand = dic;
                        ae._output_path = e_output_path;
                        ae._parent_page = pid;


                        ssvu::lo("Entry|output_path") << e_output_path << "\n";
                        ssvu::lo("Entry|template") << e_template_path << "\n";
                        ssvu::lo("Entry|expand") << e_expand_data << "\n";



                        ssvu::lo("Entry|dic|text")
                            << dic.getExpanded("{{Text}}") << "\n";

                        // Increment unique entry id.
                        ssvu::lo() << "\n";
                        ++eid;
                    }
                });

            // Increment unique page id.
            ssvu::lo() << "\n";
            ++pid;
        });


    ctx._page_mapping.for_all([&ctx](auto pid, const archetype::page& ap)
        {
            page_expansion pe;


            const auto& entry_ids = ap._entries;
            if(entry_ids.empty())
            {
                return;
            }

            // Expand permalinks
            for(auto eid : entry_ids)
            {
                auto ae = ctx._entry_mapping.get(eid);
                if(!ae._link_name)
                {
                    continue;
                }


                page_expansion permalink_pe;
                permalink_pe._subpages.emplace_back();
                auto& subpage = permalink_pe._subpages.back();


                auto e_template = Path{ae._template_path}.getContentsAsStr();
                auto e_expanded = ae._expand.getExpanded(
                    e_template, Settings::MaintainUnexisting);

                subpage._expanded_entries.emplace_back(e_expanded);
                permalink_pe.produce_result(ae._output_path);
            }

            auto entries_per_subpage = ap._subpaging
                                           ? ap._subpaging.value()
                                           : std::numeric_limits<sz_t>::max();


            auto subpage_count = entry_ids.size() / entries_per_subpage;

            utils::segmented_for(subpage_count, entries_per_subpage,
                entry_ids.size(), [&](auto, auto i_begin, auto i_end)
                {
                    pe._subpages.emplace_back();
                    auto& subpage = pe._subpages.back();

                    for(sz_t ei(i_begin); ei < i_end; ++ei)
                    {
                        std::cout << ei << " / " << entry_ids.size() << "\r";

                        auto ae = ctx._entry_mapping.get(entry_ids[ei]);

                        auto e_template =
                            Path{ae._template_path}.getContentsAsStr();
                        auto e_expanded = ae._expand.getExpanded(
                            e_template, Settings::MaintainUnexisting);

                        subpage._expanded_entries.emplace_back(e_expanded);
                    }
                });

            pe.produce_result(ap._output_path);
        });

    return 0;
}
