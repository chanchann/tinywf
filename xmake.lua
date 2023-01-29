set_project("tinywf")
set_version("0.10.0")

option("tinywf_inc",  {description = "tinywf inc", default = "$(projectdir)/_include"})
option("tinywf_lib",  {description = "tinywf lib", default = "$(projectdir)/_lib"})

if is_mode("release") then
    set_optimize("faster")
    set_strip("all")
elseif is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
end

add_includedirs("$(projectdir)")

set_languages("gnu99", "c++11")
set_warnings("all")
set_exceptions("no-cxx")

add_syslinks("pthread")

add_cflags("-fPIC", "-pipe")
add_cxxflags("-fPIC", "-pipe", "-Wno-invalid-offsetof")

add_includedirs(get_config("tinywf_inc"))
add_includedirs(path.join(get_config("tinywf_inc"), "tinywf"))

includes("src")
