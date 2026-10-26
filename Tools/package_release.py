"""Build reviewable local delivery archives and test the extracted runtime."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TASK = ROOT.parent
OUT = TASK/'交付材料'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_tree(source, target):
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))


def archive(folder, target):
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                z.write(path, Path(folder.name)/path.relative_to(folder))
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        return len(z.infolist())


def main():
    OUT.mkdir(exist_ok=True)
    latest = json.loads((ROOT/'Validation'/'latest-verification.json').read_text(encoding='utf-8-sig'))
    assert latest['status'] == 'passed'
    built = ROOT/'build-Release'/'bin'
    assert digest(built/'FarmGame.exe').upper() == latest['executableSha256']
    video = OUT/'FarmDemo_双关通关演示.mp4'
    movie = json.loads(video.with_suffix('.json').read_text(encoding='utf-8-sig'))
    assert movie['status'] == 'passed' and digest(video) == movie['videoSha256']
    assert digest(built/'FarmGame.exe') == movie['executableSha256']
    run = ROOT/'Validation'/'package'/datetime.now().strftime('%Y%m%d-%H%M%S')
    run.mkdir(parents=True)
    runtime = run/'FarmDemo_Windows'
    runtime.mkdir()
    for path in built.iterdir():
        if path.is_file() and path.suffix.lower() in ('.exe', '.dll'):
            shutil.copy2(path, runtime/path.name)
    copy_tree(built/'Resources', runtime/'Resources')
    copy_tree(TASK.parent/'cocos2d-x-3.17.2'/'licenses', runtime/'licenses')
    (runtime/'操作说明.txt').write_text(
        'Farm Demo / Windows x86 / cocos2d-x 3.17.2\n\n'
        '完整解压到可写目录，双击FarmGame.exe；同级DLL和Resources必须保留。\n'
        '菜单点击布局或按1/2；鼠标点击每列队首发车，等待车点击再次出发。\n'
        '自动收集同色临路果蔬；数字是剩余容量，满载提前驶出，未满一圈回场。\n'
        '右上角：暂停/继续、返回、重开；空格暂停，R重开，Esc返回或退出。\n'
        '圆形48块，矩形64块（含有限入口16块）；清空后显示胜利。\n'
        '使用原创占位美术，关闭合并，不含道具、扩容和额外关卡。\n'
        '在当前Windows机器实测；需要支持OpenGL的显卡驱动，尚未在另一台干净机器实测。\n'
        '运行报告位于程序旁reports/manual；当前版本无音效。\n', encoding='utf-8-sig')
    runtime_zip = OUT/'FarmDemo_可运行版.zip'
    runtime_entries = archive(runtime, runtime_zip)
    extracted = run/'解压复核'
    extracted.mkdir()
    with zipfile.ZipFile(runtime_zip) as z:
        z.extractall(extracted)
    app = extracted/runtime.name
    for path in runtime.rglob('*'):
        if path.is_file():
            assert digest(path) == digest(app/path.relative_to(runtime))
    evidence = run/'runtime-check'
    evidence.mkdir()
    unrelated = run/'独立工作目录'
    unrelated.mkdir()
    env = os.environ.copy()
    win = Path(env.get('SystemRoot', 'C:/Windows'))
    env['PATH'] = os.pathsep.join([str(win/'System32'), str(win)])
    print('Running extracted ZIP from independent working directory and minimal PATH...', flush=True)
    proc = subprocess.run([str(app/'FarmGame.exe'), '--smoke', '--report-dir', str(evidence)],
                          cwd=unrelated, env=env, check=True, timeout=50, capture_output=True)
    (evidence/'process-output.log').write_bytes(proc.stdout+proc.stderr)
    report = json.loads((evidence/'runtime-report.json').read_text(encoding='utf-8'))
    assert report['status'] == 'passed' and Path(report['resourceRoot']) == app/'Resources'
    subprocess.run([sys.executable, str(ROOT/'Tools'/'validate_runtime.py'), str(evidence)], check=True)
    source = run/'FarmDemo_Source'
    project = source/'FarmGame'
    project.mkdir(parents=True)
    for directory in ('Classes', 'proj.win32', 'Resources', 'Scripts', 'Tools', 'Previews'):
        copy_tree(ROOT/directory, project/directory)
    for name in ('CMakeLists.txt', 'README.md', '.gitignore', '启动预览.cmd', '重新编译.cmd', '完整验证.cmd'):
        shutil.copy2(ROOT/name, project/name)
    for name in ('Farm测试题_提交版.md', '源码编译说明.md', 'Farm测试题_提交版.pdf'):
        shutil.copy2(OUT/name, source/name)
    companion = source/'交付材料'
    companion.mkdir()
    shutil.copy2(OUT/'先读我.md', companion/'先读我.md')
    (source/'先读我.md').write_text(
        '# Farm源码与答题\n\n'
        '本包包含应用源码、资源、答题及验证证据。请先阅读同级源码编译说明.md。\n\n'
        'FarmGame/Classes是核心代码，FarmGame/Validation保存本次验证。\n'
        '同级Farm测试题_提交版.pdf是5页答题，Markdown保留可编辑内容。\n'
        '引擎、编译器及缓存不在此包内；可运行版和演示视频作为独立交付文件另附。\n', encoding='utf-8')
    bootstrap = source/'编译环境验证'
    bootstrap.mkdir()
    shutil.copy2(TASK/'编译环境验证'/'toolchain.ps1', bootstrap/'toolchain.ps1')
    copy_tree(runtime/'licenses', source/'licenses')
    validation = project/'Validation'
    validation.mkdir()
    for name in ('latest-verification.json', 'levels_report.json', 'assets_report.json', 'random_spawn_report.json', '通关操作.md'):
        shutil.copy2(ROOT/'Validation'/name, validation/name)
    original_run = Path(latest['runDirectory'])
    selected = validation/'runs'/latest['runId']
    selected.mkdir(parents=True)
    shutil.copy2(original_run/'verification-report.json', selected/'verification-report.json')
    copy_tree(original_run/'runtime', selected/'runtime')
    for case in ('missing_resource', 'malformed_json', 'missing_frame'):
        copy_tree(original_run/f'negative-{case}', selected/f'negative-{case}')
    copy_tree(evidence, validation/'packaged-runtime')
    demo_dir = validation/'demo-evidence'
    demo_dir.mkdir()
    shutil.copy2(movie['runtimeEvidence'], demo_dir/'runtime-report.json')
    shutil.copy2(video.with_suffix('.json'), demo_dir/'video-metadata.json')
    source_zip = OUT/'FarmDemo_源码与答题.zip'
    source_entries = archive(source, source_zip)
    manifest = dict(status='passed', createdAt=datetime.now().astimezone().isoformat(),
                    scope='v1.3 final delivery; source and video synchronized to GitHub',
                    rulesVersion='1.3.0',
                    executableSha256=digest(app/'FarmGame.exe'),
                    originalVerification=latest['runId'],
                    extractedRuntimeEvidence=str(evidence/'runtime-report.json'),
                    checks=['both ZIP CRC checks passed', 'extracted runtime file hashes match',
                            'extracted runtime starts from independent cwd with minimal PATH',
                            f'{len(report["trafficChecks"])+len(report["gameplayChecks"])} C++ check groups and {len(report["witnessObservations"])} scene launches pass in extracted runtime',
                            'extracted program resolves its own Resources directory',
                            'normal-speed video has matching executable hash and full decode check',
                            'submission PDF has five rendered and visually reviewed pages'],
                    runtimeZipEntries=runtime_entries, sourceZipEntries=source_entries,
                    videoFrames=movie['frames'], videoSeconds=movie['durationSeconds'], files=[])
    deliverables = [runtime_zip, source_zip, video, video.with_suffix('.json'), OUT/'Farm测试题_提交版.pdf', OUT/'Farm测试题_提交版.md',
                    OUT/'先读我.md', OUT/'源码编译说明.md']
    for path in deliverables:
        manifest['files'].append(dict(name=path.name, bytes=path.stat().st_size, sha256=digest(path)))
    report_path = OUT/'交付检查.json'
    report_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    deliverables.append(report_path)
    (OUT/'SHA256SUMS.txt').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in deliverables), encoding='utf-8')
    print(json.dumps(manifest, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
