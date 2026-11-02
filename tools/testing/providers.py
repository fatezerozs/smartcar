"""Suite adapters. Test assertions remain in their original C/Python sources."""
from __future__ import annotations

import glob
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from common import ROOT, inside, read_json, sha256, write_json
from process_runner import ProcessTimeout, run_process


class SuiteProblem(RuntimeError):
    def __init__(self, status, reason):
        super().__init__(reason)
        self.status = status


class Context:
    def __init__(self, options):
        self.options = options
        self.compiler_env = os.environ.copy()
        self.compiler = None
        self.tools = {'python': sys.version, 'powershell': shutil.which('pwsh'),
                      'compilerFamily': options.compiler, 'mode': options.mode}
        self.uv4 = self.find_tool(options.uv4 or os.environ.get('SMARTCAR_UV4'), 'UV4.exe', [
            r'C:\Keil_v5\UV4\UV4.exe', r'E:\ElecTronic\Keil\UV4\UV4.exe'])
        arm_bin = options.arm_bin or os.environ.get('SMARTCAR_ARM_BIN')
        self.armclang = self.find_tool(str(Path(arm_bin) / 'armclang.exe') if arm_bin else None,
                                      'armclang.exe', [r'C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe',
                                                      r'E:\ElecTronic\Keil\ARM\ARMCLANG\bin\armclang.exe'])

    @staticmethod
    def find_tool(explicit, name, candidates):
        if explicit:
            return str(Path(explicit).resolve()) if Path(explicit).is_file() else None
        return shutil.which(name) or next((p for p in candidates if Path(p).is_file()), None)


class Execution:
    def __init__(self, suite, folder, context, result, stdout, stderr):
        self.suite, self.folder, self.context, self.result = suite, folder, context, result
        self.stdout, self.stderr = stdout, stderr
        self.deadline = time.monotonic() + suite['timeoutSeconds']
        self.env = dict(os.environ, **suite['environment'])
        self.env['PYTHONDONTWRITEBYTECODE'] = '1'
        self.env['PYTHONUTF8'] = '1'
        self.work = inside(folder / 'artifacts/work', suite.get('workingDirectory','.'), must_exist=False)
        self.work.mkdir(parents=True, exist_ok=True)
        self.env['TEMP'] = self.env['TMP'] = str(self.work)

    def log(self, message):
        self.stdout.write((message + '\n').encode('utf-8'))
        self.stdout.flush()

    def process(self, command, cwd=None, env=None, stage='test', expect_failure=None):
        self.log('[' + stage + '] ' + subprocess.list2cmdline([str(s) for s in command]))
        self.stdout.flush()
        self.stderr.flush()
        offset_out, offset_err = self.stdout.tell(), self.stderr.tell()
        evidence = run_process(command, cwd or self.work, env or self.env,
                               self.deadline - time.monotonic(), self.stdout, self.stderr)
        evidence['stage'] = stage
        self.result['processes'].append(evidence)
        self.result['exitCode'] = evidence['exitCode']
        if not evidence['cleanupVerified']:
            raise SuiteProblem('ERROR', 'Process tree cleanup could not be verified')
        if evidence['timedOut']:
            raise SuiteProblem('TIMEOUT', 'Suite deadline expired during ' + stage)
        if expect_failure:
            self.stdout.flush()
            self.stderr.flush()
            text = (self.folder / 'stdout.log').read_bytes()[offset_out:] + (self.folder / 'stderr.log').read_bytes()[offset_err:]
            text = text.decode('utf-8', errors='replace')
            # A compiler crash, missing header/tool or unrelated error is never a passing negative test.
            guard_lines = [line for line in text.splitlines()
                           if expect_failure in line and ('error C1189' in line or '#error' in line)]
            if evidence['exitCode'] == 0 or not guard_lines:
                raise SuiteProblem('FAIL', 'Invalid configuration did not trigger its expected production #error: ' + expect_failure)
            self.log('EXPECTED CONFIGURATION REJECTION: ' + expect_failure)
        elif evidence['exitCode']:
            raise SuiteProblem('ERROR' if stage in ('compile','toolchain','build') else 'FAIL',
                               stage + ' exited with code ' + str(evidence['exitCode']))
        return evidence

    def copy_workspace(self):
        workspace = self.folder / 'artifacts/workspace'
        for name in ('demo', 'libraries'):
            shutil.copytree(ROOT / name, workspace / name,
                            ignore=shutil.ignore_patterns('Objects','Listings','*.uvoptx','*.uvguix.*','*.build_log.htm'))
        return workspace

    def initialize_compiler(self):
        context = self.context
        env = dict(context.compiler_env)
        if context.options.compiler == 'gcc':
            compiler = shutil.which('gcc')
            if not compiler:
                raise SuiteProblem('BLOCKED', 'GCC is unavailable')
        else:
            compiler = shutil.which('cl.exe')
            if not compiler:
                vcvars = context.options.vcvars or os.environ.get('SMARTCAR_VCVARS')
                if not vcvars:
                    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
                    if vswhere.is_file():
                        proc = subprocess.run([str(vswhere), '-latest','-products','*','-requires',
                                               'Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],
                                              capture_output=True,text=True,timeout=min(20,max(.01,self.deadline-time.monotonic())),
                                              creationflags=subprocess.CREATE_NO_WINDOW)
                        install = proc.stdout.strip()
                        if install:
                            vcvars = str(Path(install) / 'VC/Auxiliary/Build/vcvars64.bat')
                    if not vcvars:
                        candidates = []
                        for base in (os.environ.get('ProgramFiles', r'C:\Program Files'),
                                     os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')):
                            candidates += glob.glob(str(Path(base) / 'Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat'))
                        vcvars = sorted(candidates)[-1] if candidates else None
                if not vcvars or not Path(vcvars).is_file():
                    raise SuiteProblem('BLOCKED', 'MSVC unavailable; install C++ Build Tools or supply -VcVars / SMARTCAR_VCVARS')
                if any(c in vcvars for c in '"\r\n%'):
                    raise SuiteProblem('ERROR', 'Invalid vcvars path')
                # This batch contains only a discovered/user-configured vcvars path and a fixed SET.
                batch = self.work / 'compiler-env.cmd'
                batch.write_text('@echo off\ncall "' + vcvars + '" >nul\nif errorlevel 1 exit /b 2\nset\n',encoding='utf-8')
                capture = self.work / 'compiler-env.txt'
                with capture.open('wb') as out:
                    evidence = run_process(['cmd','/d','/c',str(batch)],self.work,self.env,
                                           min(60,self.deadline-time.monotonic()),out,self.stderr)
                if evidence['exitCode'] or evidence['timedOut'] or not evidence['cleanupVerified']:
                    raise SuiteProblem('ERROR','MSVC environment initialization failed')
                for line in capture.read_text(encoding='utf-8',errors='replace').splitlines():
                    if '=' in line and not line.startswith('='):
                        key,value = line.split('=',1)
                        # Windows environment names are case-insensitive.
                        for existing in list(env):
                            if existing.upper() == key.upper():
                                del env[existing]
                        env[key] = value
                capture.unlink()  # Do not retain a dump of the user's environment.
                compiler = shutil.which('cl.exe',path=next((v for k,v in env.items() if k.upper()=='PATH'),''))
            if not compiler:
                raise SuiteProblem('BLOCKED','cl.exe not found after MSVC environment initialization')
        context.compiler, context.compiler_env = compiler, env
        context.tools['compilerPath'] = compiler
        self.process([compiler, '--version'] if context.options.compiler == 'gcc' else [compiler,'/Bv'],
                     env=env, stage='toolchain') if context.options.compiler == 'gcc' else None
        probe = self.work / 'environment_probe.c'
        probe.write_text('int main(void) { return 0; }\n',encoding='ascii')
        exe = self.work / 'environment_probe.exe'
        self.process(self.compile_command([probe], [], [], exe), env=env, stage='compile')
        self.process([exe], env=env)
        log = (self.folder / 'stdout.log').read_text(encoding='utf-8',errors='replace')
        err = (self.folder / 'stderr.log').read_text(encoding='utf-8',errors='replace')
        context.tools['compilerVersionEvidence'] = '\n'.join((log+'\n'+err).splitlines()[:16])

    def compile_command(self, sources, includes, defines, output, *, object_only=False):
        context = self.context
        if not context.compiler:
            raise SuiteProblem('BLOCKED','Compiler environment has not passed')
        if context.options.compiler == 'msvc':
            flags = ['/nologo','/utf-8','/std:c11','/W4','/WX']
            flags += ['/Od','/Zi','/fsanitize=address'] if context.options.mode == 'asan' else ['/O2']
            flags += ['/I'+str(p) for p in includes] + ['/D'+d for d in defines]
            flags += [str(p) for p in sources]
            flags += ['/c','/Fo'+str(output)] if object_only else ['/Fo'+str(output.parent)+os.sep,
                     '/Fd'+str(output.parent)+os.sep, '/Fe'+str(output)]
        else:
            # GCC is an explicitly selected, supplemental Host toolchain. No production flags are changed.
            flags = ['-std=c11','-Wall','-Werror','-Wno-misleading-indentation','-Wno-unused-function']
            flags += ['-O0','-g','-fsanitize=address'] if context.options.mode == 'asan' else ['-O2']
            flags += ['-I'+str(p) for p in includes] + ['-D'+d for d in defines]
            flags += [str(p) for p in sources]
            if object_only:
                flags += ['-c']
            flags += ['-o',str(output)]
            if not object_only:
                flags += ['-lm']
        return [context.compiler] + flags

    def run_c(self):
        entry = self.suite['entrypoint']
        for variant in entry['variants']:
            work = inside(self.folder / 'artifacts' / variant['id'],self.suite['workingDirectory'],must_exist=False)
            work.mkdir(parents=True)
            self.result['variants'].append(dict(variant, mode=self.context.options.mode,
                                                compilerFamily=self.context.options.compiler))
            for fixture in entry.get('fixtures', []):
                shutil.copytree(inside(ROOT,fixture),work / Path(fixture).name)
            sources = [inside(ROOT,p) for p in variant['sources']]
            includes = [inside(ROOT,p) for p in variant['includeDirectories']]
            env = dict(self.context.compiler_env, **self.suite['environment'])
            env.update(TEMP=str(work),TMP=str(work))
            if 'reference' in variant:
                reference = variant['reference']
                obj = work / 'reference.obj'
                self.process(self.compile_command([inside(ROOT,reference['source'])], includes,
                                                   variant['defines']+reference['defines'],obj,object_only=True),
                             cwd=work,env=env,stage='compile')
                sources.append(obj)
            exe = work / (self.suite['id']+'.exe')
            self.process(self.compile_command(sources,includes,variant['defines'],exe),cwd=work,env=env,
                         stage='compile',expect_failure=variant.get('expectCompileFailure'))
            if not variant.get('expectCompileFailure'):
                self.process([exe],cwd=work,env=env)

    def run_keil(self):
        if not self.context.uv4:
            raise SuiteProblem('BLOCKED','Keil UV4 is unavailable; supply -UV4 or SMARTCAR_UV4')
        workspace = self.copy_workspace()
        project = workspace / 'demo/mdk/base/rt1064.uvprojx'
        tree = ET.parse(project)
        targets = [t for t in tree.findall('./Targets/Target') if t.findtext('TargetName')=='nor_sdram_zf_dtcm']
        if len(targets)!=1:
            raise SuiteProblem('ERROR','Expected one Keil target')
        target = targets[0]
        defines = target.find('./TargetOption/TargetArmAds/Cads/VariousControls/Define')
        added = self.suite['entrypoint'].get('defines',[])
        reserved = {s.split('=')[0] for s in added}
        inherited = [s.strip() for s in (defines.text or '').split(',') if s.strip()]
        if any(s.split('=')[0].strip() in reserved for s in inherited):
            raise SuiteProblem('ERROR','Inherited target overrides a variant policy macro')
        defines.text = ','.join(added+inherited)
        for key, value in [('OutputDirectory','Objects'),('ListingPath','Listings')]:
            directory = workspace / value
            directory.mkdir()
            target.find('./TargetOption/TargetCommonOption/'+key).text = str(directory)+os.sep
        tree.write(project,encoding='utf-8',xml_declaration=True)
        log = self.folder / 'artifacts/build.log'
        started = time.time()
        self.process([self.context.uv4,'-r',project,'-t','nor_sdram_zf_dtcm','-j0','-o',log],
                     cwd=project.parent,stage='build')
        self.result['variants'].append({'id':self.suite['id'],'defines':added,'inheritedDefines':inherited,
                                        'projectSha256':sha256(project)})
        artifact_name = target.findtext('./TargetOption/TargetCommonOption/OutputName') or 'rt1064'
        artifact = workspace / 'Objects' / (artifact_name+'.axf')
        if not log.is_file() or not artifact.is_file() or artifact.stat().st_mtime < started-2:
            raise SuiteProblem('ERROR','Missing or stale Keil build log/AXF')
        build_log = log.read_text(encoding='utf-8',errors='replace')
        self.log(build_log)
        if not re.search(r'0 Error\(s\), 0 Warning\(s\)',build_log):
            raise SuiteProblem('FAIL','Keil build must report zero errors and warnings')
        self.result['details']['axfSha256'] = sha256(artifact)
        self.result['details']['buildOnly'] = True

    def run_static(self):
        forbidden = []
        for path in (ROOT/'tests').rglob('*.c'):
            if re.search(r'#\s*include\s+"[^"\n]*test_[^"\n]*\.c"',path.read_text(encoding='utf-8')):
                forbidden.append(str(path.relative_to(ROOT)))
        if forbidden:
            raise SuiteProblem('FAIL','Tests include other test source files: '+', '.join(forbidden))
        fixture = read_json(ROOT/'tests/vision/fixtures/tangent-preview/manifest.json')
        for item in fixture['files']:
            path = ROOT / 'tests/vision/fixtures/tangent-preview' / ('frame-%06d.gray'%item['frame'])
            if sha256(path) != item['sha256'] or path.stat().st_size != fixture['width']*fixture['height']:
                raise SuiteProblem('FAIL','Preview fixture content/dimensions changed: '+path.name)
        for path in (ROOT/'demo/mdk').rglob('*.uvprojx'):
            xml = ET.parse(path)
            for node in xml.findall('.//FilePath'):
                source = (path.parent / node.text.replace('\\','/')).resolve()
                if not source.is_relative_to(ROOT) or not source.is_file():
                    raise SuiteProblem('FAIL','Missing/escaping Keil input: '+str(source))
                if source.is_relative_to(ROOT/'tests'):
                    raise SuiteProblem('FAIL','Host test source referenced by a target project')
        writers = [p.name for p in (ROOT/'demo/code').glob('*.c')
                   if re.search(r'\bpwm_set_duty\s*\(',p.read_text(encoding='utf-8'))]
        if writers != ['vision_actuator.c']:
            raise SuiteProblem('FAIL','Unexpected production PWM writer set: '+repr(writers))
        self.log('PASS: independent test sources; fixture hashes/dimensions; all Keil source paths; PWM writer ownership.')
        self.result['details']['manualReviewPending'] = ['ISR/DMA ownership and critical-section correctness',
                                                        'PWM/LDOK physical timing and worst-case execution time']

    def execute(self):
        kind = self.suite['entrypoint']['type']
        if kind == 'environment':
            if sys.version_info < (3,10) or os.name != 'nt' or not shutil.which('pwsh') or not shutil.which('git'):
                raise SuiteProblem('BLOCKED','Windows, Python >=3.10, PowerShell 7 and Git are required')
            self.process([shutil.which('pwsh'),'-NoProfile','-Command','$PSVersionTable.PSVersion.ToString()'])
        elif kind == 'compiler-environment':
            self.initialize_compiler()
        elif kind in ('target-environment','arm-environment'):
            tool = self.context.uv4 if kind == 'target-environment' else self.context.armclang
            if not tool:
                raise SuiteProblem('BLOCKED','Missing '+kind+' tool; configure -UV4 / -ArmBin')
            self.result['details']['toolPath'] = tool
            self.result['details']['toolSha256'] = sha256(tool)
            self.process([shutil.which('pwsh'),'-NoProfile','-File',ROOT/'tools/testing/Get-ToolVersion.ps1',
                          '-Path',tool],stage='toolchain')
            self.context.tools[kind] = self.result['details']
        elif kind == 'c':
            self.run_c()
        elif kind in ('python','powershell','cmd','native'):
            entry = self.suite['entrypoint']
            path = inside(ROOT,entry['path'])
            if kind == 'python':
                command = [sys.executable,'-B',str(path)]
            elif kind == 'powershell':
                command = [shutil.which('pwsh'),'-NoLogo','-NoProfile','-NonInteractive','-File',str(path)]
            elif kind == 'cmd':
                command = ['cmd','/d','/c',str(path)]
            else:
                command = [str(path)]
            command += entry.get('arguments',[])
            # Code locates repository files via __file__/$PSScriptRoot. CWD is private and writable.
            self.process(command)
        elif kind == 'aggregate':
            self.log('PASS: all explicit child suite dependencies passed')
        elif kind == 'static':
            self.run_static()
        elif kind == 'keil':
            self.run_keil()
        elif kind == 'arm':
            if not self.context.armclang:
                raise SuiteProblem('BLOCKED','ARMCLANG is unavailable')
            workspace = self.copy_workspace()
            (workspace/'tools').mkdir()
            shutil.copy2(ROOT/'tools/check_arm.ps1',workspace/'tools/check_arm.ps1')
            self.process([shutil.which('pwsh'),'-NoProfile','-NonInteractive','-File',
                          workspace/'tools/check_arm.ps1','-ArmBin',str(Path(self.context.armclang).parent)],stage='compile')
        elif kind == 'benchmark':
            from benchmark import validate_benchmark
            spec = read_json(inside(ROOT,self.suite['entrypoint']['path']))
            validate_benchmark(spec)
            if spec['status'] == 'pending':
                raise SuiteProblem('SKIPPED',spec['reason'])
            # Measurements must be supplied explicitly; an adapter is never guessed or silently executed.
            raise SuiteProblem('BLOCKED','Benchmark is ready; supply measurements through Invoke-Benchmark.ps1')
        elif kind == 'manual':
            raise SuiteProblem('MANUAL_PENDING','Independent hardware authorization, device setup and measurement evidence required')
        else:
            raise SuiteProblem('ERROR','Unsupported entrypoint: '+kind)
