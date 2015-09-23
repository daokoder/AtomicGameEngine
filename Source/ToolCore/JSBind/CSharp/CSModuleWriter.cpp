//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// LICENSE: Atomic Game Engine Editor and Tools EULA
// Please see LICENSE_ATOMIC_EDITOR_AND_TOOLS.md in repository root for
// license information: https://github.com/AtomicGameEngine/AtomicGameEngine
//

#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>

#include "../JSBind.h"
#include "../JSBPackage.h"
#include "../JSBModule.h"
#include "../JSBEnum.h"
#include "../JSBClass.h"
#include "../JSBFunction.h"

#include "CSClassWriter.h"
#include "CSModuleWriter.h"

namespace ToolCore
{

CSModuleWriter::CSModuleWriter(JSBModule *module) : JSBModuleWriter(module)
{

}

void CSModuleWriter::WriteIncludes(String& source)
{

    Vector<String>& includes = module_->includes_;
    for (unsigned i = 0; i < includes.Size(); i++)
    {
        if (includes[i].StartsWith("<"))
            source.AppendWithFormat("#include %s\n", includes[i].CString());
        else
            source.AppendWithFormat("#include \"%s\"\n", includes[i].CString());
    }

    Vector<JSBHeader*> allheaders;

    HashMap<StringHash, SharedPtr<JSBEnum> >::Iterator eitr = module_->enums_.Begin();
    while (eitr != module_->enums_.End())
    {
        allheaders.Push(eitr->second_->GetHeader());
        eitr++;
    }

    HashMap<StringHash, SharedPtr<JSBClass> >::Iterator citr = module_->classes_.Begin();
    while (citr != module_->classes_.End())
    {
        allheaders.Push(citr->second_->GetHeader());
        citr++;
    }

    Vector<JSBHeader*> included;

    for (unsigned i = 0; i < allheaders.Size(); i++)
    {
        JSBHeader* header = allheaders.At(i);

        if (included.Contains(header))
            continue;

        String headerPath = GetPath(header->GetFilePath());

        String headerfile = GetFileNameAndExtension(header->GetFilePath());

        JSBind* jsbind = header->GetSubsystem<JSBind>();

        headerPath.Replace(jsbind->GetSourceRootFolder() + "Source/", "");

        source.AppendWithFormat("#include <%s%s>\n", headerPath.CString(), headerfile.CString());

        included.Push(header);
    }

}


void CSModuleWriter::GenerateNativeSource()
{
    String source = "// This file was autogenerated by JSBind, changes will be lost\n";

    source += "#ifdef ATOMIC_PLATFORM_WINDOWS\n";

    source += "#pragma warning(disable: 4244) // possible loss of data\n";

    source += "#endif\n";

    if (module_->Requires("3D"))
    {
        source += "#ifdef ATOMIC_3D\n";
    }

    WriteIncludes(source);

    source += "\n#include <AtomicSharp/AtomicSharp.h>\n";

    String ns = module_->GetPackage()->GetNamespace();

    source += "\n\nusing namespace " + ns + ";\n\n";

    source += "\n\nextern \"C\" \n{\n \n";

    source += "// Begin Class Declarations\n";

    source += "// End Class Declarations\n\n";

    source += "// Begin Classes\n";

    Vector<SharedPtr<JSBClass>> classes = module_->classes_.Values();

    for (unsigned i = 0; i < classes.Size(); i++)
    {
        CSClassWriter clsWriter(classes[i]);
        clsWriter.GenerateSource(source);
    }

    source += "// End Classes\n\n";

    // end Atomic namespace
    source += "\n}\n";

    if (module_->Requires("3D"))
    {
        source += "#endif //ATOMIC_3D\n";
    }

    JSBind* jsbind = module_->GetSubsystem<JSBind>();

    String filepath = jsbind->GetDestNativeFolder() + "/CSModule" + module_->name_ + ".cpp";

    File file(module_->GetContext());
    file.Open(filepath, FILE_WRITE);
    file.Write(source.CString(), source.Length());
    file.Close();

}

String CSModuleWriter::GetManagedPrimitiveType(JSBPrimitiveType* ptype)
{
    if (ptype->kind_ == JSBPrimitiveType::Bool)
        return "bool";
    if (ptype->kind_ == JSBPrimitiveType::Int && ptype->isUnsigned_)
        return "uint";
    else if (ptype->kind_ == JSBPrimitiveType::Int)
        return "int";
    if (ptype->kind_ == JSBPrimitiveType::Float)
        return "float";

    return "int";
}


void CSModuleWriter::GenerateManagedEnumsAndConstants(String& source)
{
    Vector<SharedPtr<JSBEnum>> enums = module_->enums_.Values();

    Indent();

    for (unsigned i = 0; i < enums.Size(); i++)
    {
        JSBEnum* jenum = enums[i];

        source += "\n";
        String line = "public enum " + jenum->GetName() + "\n";
        source += IndentLine(line);
        source += IndentLine("{\n");

        HashMap<String, String>& values = jenum->GetValues();

        HashMap<String, String>::ConstIterator itr = values.Begin();

        Indent();

        while (itr != values.End())
        {
            String name = (*itr).first_;
            String value = (*itr).second_;

            if (value.Length())
            {
                line = name + " = " + value;
            }
            else
            {
                line = name;
            }

            itr++;

            if (itr != values.End())
                line += ",";

            line += "\n";

            source += IndentLine(line);

        }

        source += "\n";

        Dedent();

        source += IndentLine("}\n");

    }

    // constants

    HashMap<String, JSBModule::Constant>& constants = module_->GetConstants();

    if (constants.Size())
    {

        source += "\n";

        String line = "public static class Constants\n";
        source += IndentLine(line);
        source += IndentLine("{\n");

        const Vector<String>& constantsName = constants.Keys();

        Indent();

        for (unsigned i = 0; i < constantsName.Size(); i++)
        {
            const String& cname = constantsName.At(i);

            JSBModule::Constant& constant = constants[cname];

            String managedType = GetManagedPrimitiveType(constant.type);

            String value = constant.value;
            //static const unsigned M_MIN_UNSIGNED = 0x00000000;
//            /static const unsigned M_MAX_UNSIGNED = 0xffffffff;

            if (value == "M_MAX_UNSIGNED")
                value = "0xffffffff";

            String line = "public static " + managedType + " " + cname + " = " + value;

            if (managedType == "float" && !line.EndsWith("f"))
                line += "f";

            line += ";\n";

            source += IndentLine(line);

        }

        Dedent();

        source += "\n";
        line = "}\n";
        source += IndentLine(line);

    }


    Dedent();

}

void CSModuleWriter::GenerateManagedSource()
{
    String source;

    source += "namespace " + module_->GetPackage()->GetName() + "\n";
    source += "{\n";

    GenerateManagedEnumsAndConstants(source);

    source += "}\n";


    JSBind* jsbind = module_->GetSubsystem<JSBind>();
    String filepath = jsbind->GetDestScriptFolder() + "/CSModule" + module_->name_ + ".cs";

    File file(module_->GetContext());
    file.Open(filepath, FILE_WRITE);
    file.Write(source.CString(), source.Length());
    file.Close();

}

void CSModuleWriter::GenerateSource()
{   
    GenerateNativeSource();
    GenerateManagedSource();
}

}
