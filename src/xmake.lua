target("tinywf")
    set_kind("static")
    add_files("**.c")
    add_files("**.cc")

    on_load(function (package)
        local include_path = path.join(get_config("tinywf_inc"), "tinywf")
        if (not os.isdir(include_path)) then
            os.mkdir(include_path)
        end

        os.cp(path.join("$(projectdir)", "src/**.h"), include_path)
    end)

    after_clean(function (target)
        os.rm(get_config("tinywf_inc"))
        os.rm(get_config("tinywf_lib"))
        os.rm("$(buildir)")
    end)