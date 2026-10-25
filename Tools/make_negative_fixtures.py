"""Create isolated broken packages; never mutate authored or built resources."""
import argparse
import json
import shutil
from pathlib import Path

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('source',type=Path)
    p.add_argument('destination',type=Path)
    args=p.parse_args()
    root=args.destination
    root.mkdir(parents=True,exist_ok=False)
    (root/'missing_resource').mkdir()
    for name in ['malformed_json','missing_frame']:
        shutil.copytree(args.source,root/name)
    (root/'malformed_json'/'config/rules.v1.json').write_text('{"rulesVersion":',encoding='utf-8')
    path=root/'missing_frame'/'art/asset_manifest.json'
    manifest=json.loads(path.read_text(encoding='utf-8'))
    manifest['assets']['fruit.green']['frame']='intentionally_absent_frame.png'
    path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Prepared missing_resource, malformed_json and missing_frame fixtures.')
