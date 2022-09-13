import glob
import os
import sys
import json


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
                    test_path = os.path.join("tests", "xml", test_type)
                    folder = data.get('folder')
                    if folder is not None:
                        test_path = os.path.join(test_path, folder)
                    f.write("TEST_CASE(\"" + data['name'] + "\") {\n")
                    base_name = os.path.splitext(os.path.basename(file))[0]
                    base_name_input = os.path.join(test_path,
                                                   base_name + "_input.xml")
                    base_name_patch = os.path.join(test_path,
                                                   base_name + "_patch.xml")
                    f.write("TestRunner runner(\"%s\", \"%s\", \"%s\");\n" %
                            (os.path.join("tests", "xml", test_type).replace(
                                "\\", "/"), base_name_input.replace("\\", "/"),
                             base_name_patch.replace("\\", "/")))
                    f.write("runner.ApplyPatches();\n")
                    f.write("INFO(runner.DumpXml());")
                    expected_paths = data['expected']
                    for expected_path in expected_paths:
                        if expected_path.startswith('!'):
                            f.write("CHECK_FALSE(runner.PathExists(\"" +
                                    expected_path[1:] + "\"));")
                        else:
                            f.write("CHECK(runner.PathExists(\"" +
                                    expected_path + "\"));")
                    f.write("}\n\n")


if __name__ == "__main__":
    main()
