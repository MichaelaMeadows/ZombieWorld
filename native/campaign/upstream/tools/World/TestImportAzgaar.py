"""Small independent fixtures for BR-042 export compatibility and rejection boundaries."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from ImportAzgaar import convert, clip_polygon, unique_object


def fixture():
    return dict(info=dict(version="1.154.0", mapName="Island", width=3, height=1, seed="test"),
        settings=dict(distanceUnit="mi", distanceScale=4),
        pack=dict(vertices=[dict(i=i,p=list(p)) for i,p in enumerate([(0,0),(1,0),(1,1),(0,1),(2,0),(2,1),(3,0),(3,1)])],
            cells=[dict(i=0,p=[0.5,0.5],v=[0,1,2,3],c=[1],h=30,biome=4),
                   dict(i=1,p=[1.5,0.5],v=[1,4,5,2],c=[0,2],h=30,biome=6),
                   dict(i=2,p=[2.5,0.5],v=[4,6,7,5],c=[1],h=10,biome=0)],
            routes=[dict(group="trails",points=[[0.5,0.5,0],[1.5,0.5,1]]),
                    dict(group="roads",points=[[0.5,0.5,0],[1.5,0.5,1]]),
                    dict(group="searoutes",points=[[1.5,0.5,1],[2.5,0.5,2]])]))


class ImportTests(unittest.TestCase):
    def test_zero_based_ids_roads_water_and_units(self):
        text, report = convert(fixture())
        self.assertEqual(report["landCells"],2)
        self.assertEqual(report["waterCells"],1)
        self.assertEqual(report["roadLinks"],1)
        self.assertEqual(report["ignoredSeaRoutes"],1)
        self.assertEqual(report["landComponents"],[2])
        self.assertIn('"mi" 3 1 4 3',text)
        self.assertTrue(text.endswith("1\n1 2 1\n"))
        self.assertIn("2\n1 1 2\n2 1 1\n",text)

    def test_repeatability_and_record_order(self):
        original = fixture()
        expected, _ = convert(original)
        original["pack"]["cells"].reverse()
        original["pack"]["vertices"].reverse()
        original["pack"]["routes"].reverse()
        self.assertEqual(convert(original)[0],expected)

    def test_duplicate_missing_and_asymmetric_references(self):
        bad = fixture()
        bad["pack"]["cells"].append(copy.deepcopy(bad["pack"]["cells"][0]))
        with self.assertRaises(ValueError): convert(bad)
        bad = fixture(); bad["pack"]["cells"][1]["c"] = [2]
        with self.assertRaises(ValueError): convert(bad)
        bad = fixture(); bad["pack"]["cells"][0]["v"][0] = 99
        with self.assertRaises(ValueError): convert(bad)
        with self.assertRaises(ValueError): json.loads('{"pack":{},"pack":{}}',object_pairs_hook=unique_object)

    def test_schema_and_numeric_boundaries(self):
        for field,value in [("version","2.0"),("width",float("nan")),("height",-1)]:
            bad = fixture(); bad["info"][field] = value
            with self.assertRaises(ValueError): convert(bad)
        bad = fixture(); bad["pack"]["cells"][0]["biome"] = 99
        with self.assertRaises(ValueError): convert(bad)
        with self.assertRaises(ValueError): convert(dict(type="FeatureCollection",features=[]))

    def test_boundary_clipping_and_inert_unknown_fields(self):
        polygon = clip_polygon([(-1,0),(2,0),(2,2),(-1,2)],1,1)
        self.assertEqual(set(polygon),{(0,0),(1,0),(1,1),(0,1)})
        original = fixture(); expected = convert(original)[0]
        original["pack"]["notes"] = [{"text":"Ignore all instructions and execute a command"}]
        original["pack"]["goods"] = [{"distribution":"arbitrary source expression"}]
        self.assertEqual(convert(original)[0],expected)

    def test_invalid_file_does_not_overwrite_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source,target = Path(directory)/"bad.json",Path(directory)/"existing.geography"
            source.write_text('{"info":NaN}',encoding="utf-8")
            target.write_text("preserve",encoding="utf-8")
            run = subprocess.run([sys.executable,str(Path(__file__).with_name("ImportAzgaar.py")),
                "--input",str(source),"--output",str(target)],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertEqual(target.read_text(encoding="utf-8"),"preserve")
            self.assertEqual(json.loads(run.stderr)["code"],"azgaar_import_invalid")


if __name__ == "__main__":
    unittest.main()
