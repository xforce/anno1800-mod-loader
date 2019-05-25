import glob
import os
import sys
import json
from functools import partial


def main():
    # Gather tests
    with open(sys.argv[1], 'w+') as f:
        f.write("#include \"runner.h\"\n")
        tests_directory = os.path.dirname(os.path.abspath(__file__))
        for test_type in os.listdir(tests_directory):
            test_type_dir = os.path.join(tests_directory, test_type)
            if os.path.isdir(test_type_dir):
                for file in glob.glob(os.path.join(test_type_dir, "*.json")):
                    with open(file, "r") as json_file:
                        data = json.load(json_file)
                    f.write("TEST_CASE(\"" + data['name'] + "\") {\n")
                    base_name = os.path.splitext(os.path.basename(file))[0]
                    base_name_input = os.path.join(
                        test_type_dir, base_name + "_input.xml")
                    base_name_patch = os.path.join(
                        test_type_dir, base_name + "_patch.xml")
                    with open(base_name_input, "r") as in_file:
                        f.write("char input[] = {\n")
                        content = "<MEOW_XML_SUCKS>" + in_file.read() + "</MEOW_XML_SUCKS>"
                        for c in content:
                            if c != '':
                                f.write("0x%02X," % ord(c))
                        f.write("0x%02X," % ord('\0'))
                        f.write("};\n")
                    with open(base_name_patch, "r") as in_file:
                        f.write("char patch[] = {\n")
                        content = in_file.read()
                        for c in content:
                            if c != '':
                                f.write("0x%02X," % ord(c))
                        f.write("0x%02X," % ord('\0'))
                        f.write("};\n")
                    f.write("TestRunner runner(input, patch);\n")
                    f.write("runner.ApplyPatches();\n")
                    f.write("INFO(runner.DumpXml());")
                    expected_paths = data['expected']
                    for expected_path in expected_paths:
                        f.write(
                            "CHECK(runner.PathExists(\"" + expected_path + "\"));")
                    f.write("}\n\n")


if __name__ == "__main__":
    main()
