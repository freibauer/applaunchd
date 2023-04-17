Interface definitions captured with:

gdbus introspect --system -x --dest org.freedesktop.systemd1 --object-path /org/freedesktop/systemd1

and then pruned down a bit by hand to just the interfaces required.

Code generated with:

gdbus-codegen --interface-prefix org.freedesktop. --generate-c-code systemd1_manager_interface --c-generate-object-manager org_freedesktop_systemd1_manager.xml 
gdbus-codegen --interface-prefix org.freedesktop. --generate-c-code systemd1_unit_interface org_freedesktop_systemd1_unit.xml 

