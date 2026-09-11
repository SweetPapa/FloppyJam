#!/usr/bin/env python3
"""Deterministic, dependency-free Xcode project. Run after adding source files."""
import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]/'ios'
project=root/'Maglava.xcodeproj'; project.mkdir(exist_ok=True)
def uid(s): return hashlib.sha1(s.encode()).hexdigest()[:24].upper()
def q(s): return json.dumps(s)
objects=[]
def obj(name,body): objects.append(f'{uid(name)} = {{ {body} }};'); return uid(name)
files=[('Maglava/Soundtrack.swift','sourcecode.swift','source'),('MaglavaTests/MaglavaTests.swift','sourcecode.swift','test'),('Maglava/App.swift','sourcecode.swift','source'),('Maglava/GameScene.swift','sourcecode.swift','source'),('../shared/mobile_core.c','sourcecode.c.c','source'),('../shared/presentation.c','sourcecode.c.c','source'),('../shared/levels.c','sourcecode.c.c','source'),('../../v4/src/sim.c','sourcecode.c.c','source'),('Maglava/Bridge.h','sourcecode.c.h',''),('Maglava/Info.plist','text.plist.xml',''),('Maglava/PrivacyInfo.xcprivacy','text.xml','resource'),('../../v4/assets/fonts/Barlow-Medium.ttf','file','resource'),('../../v4/assets/fonts/Barlow-SemiBold.ttf','file','resource'),('../../v4/assets/fonts/OFL.txt','text','resource'),('../assets/Audio','folder','resource'),('Maglava/Assets.xcassets','folder.assetcatalog','resource')]
refs=[]; sources=[]; resources=[]; tests=[]
for path,kind,phase in files:
    ref=obj(path,f'isa = PBXFileReference; lastKnownFileType = {q(kind)}; path = {q(path)}; sourceTree = "<group>";');refs.append(ref)
    if phase:
        build=obj('build'+path,f'isa = PBXBuildFile; fileRef = {ref};')
        (sources if phase=='source' else tests if phase=='test' else resources).append(build)
product=obj('product','isa = PBXFileReference; explicitFileType = wrapper.application; path = Maglava.app; sourceTree = BUILT_PRODUCTS_DIR;')
products=obj('products',f'isa = PBXGroup; children = ({product},); name = Products; sourceTree = "<group>";')
main=obj('main',f'isa = PBXGroup; children = ({",".join(refs+[products])},); sourceTree = "<group>";')
sourcephase=obj('sources',f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(sources)},); runOnlyForDeploymentPostprocessing = 0;')
resourcephase=obj('resources',f'isa = PBXResourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(resources)},); runOnlyForDeploymentPostprocessing = 0;')
frameworkphase=obj('frameworks','isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0;')
script='set -eu\nexport PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"\npython3 "$SRCROOT/../tools/generate_levels.py" "$DERIVED_FILE_DIR/levels_gen.h"\n'
genphase=obj('generate',f'isa = PBXShellScriptBuildPhase; buildActionMask = 2147483647; files = (); inputPaths = (); outputPaths = ("$(DERIVED_FILE_DIR)/levels_gen.h",); alwaysOutOfDate = 1; name = "Compile canonical level JSON"; shellPath = /bin/sh; shellScript = {q(script)}; runOnlyForDeploymentPostprocessing = 0;')
projconfigs=[]; targetconfigs=[]
for name in ['Debug','Release']:
    debug=name=='Debug'
    projconfigs.append(obj('project'+name,f'isa = XCBuildConfiguration; name = {name}; buildSettings = {{ SDKROOT = iphoneos; IPHONEOS_DEPLOYMENT_TARGET = 16.0; CLANG_ENABLE_MODULES = YES; CLANG_ENABLE_OBJC_ARC = YES; GCC_C_LANGUAGE_STANDARD = c99; SWIFT_VERSION = 5.0; DEBUG_INFORMATION_FORMAT = {q("dwarf" if debug else "dwarf-with-dsym")}; GCC_OPTIMIZATION_LEVEL = {"0" if debug else "s"}; }};'))
    settings={'PRODUCT_NAME':'Maglava','PRODUCT_BUNDLE_IDENTIFIER':'dev.fofo.maglava','INFOPLIST_FILE':'Maglava/Info.plist','SWIFT_OBJC_BRIDGING_HEADER':'Maglava/Bridge.h','TARGETED_DEVICE_FAMILY':'1,2','CODE_SIGN_STYLE':'Automatic','ENABLE_USER_SCRIPT_SANDBOXING':'NO','ENABLE_TESTABILITY':'YES' if debug else 'NO','SWIFT_OPTIMIZATION_LEVEL':'-Onone' if debug else '-O','SWIFT_ACTIVE_COMPILATION_CONDITIONS':'DEBUG' if debug else '', 'ASSETCATALOG_COMPILER_APPICON_NAME':'AppIcon','SUPPORTED_PLATFORMS':'iphoneos iphonesimulator macosx','SUPPORTS_MACCATALYST':'YES','DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER':'NO','MACOSX_DEPLOYMENT_TARGET':'13.0','DEVELOPMENT_TEAM':'6Y5SZ2K5XY'}
    body=' '.join(f'{k} = {q(v)};' for k,v in settings.items())
    body+=' \"CODE_SIGN_ENTITLEMENTS[sdk=macosx*]\" = Maglava/Maglava.entitlements;'
    body+=' HEADER_SEARCH_PATHS = ("$(inherited)", "$(SRCROOT)/../shared", "$(DERIVED_FILE_DIR)", "$(SRCROOT)/../../v4/src");'
    targetconfigs.append(obj('target'+name,f'isa = XCBuildConfiguration; name = {name}; buildSettings = {{ {body} }};'))
for name,configs in [('projectlist',projconfigs),('targetlist',targetconfigs)]:obj(name,f'isa = XCConfigurationList; buildConfigurations = ({",".join(configs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
target=obj('target',f'isa = PBXNativeTarget; buildConfigurationList = {uid("targetlist")}; buildPhases = ({genphase},{sourcephase},{frameworkphase},{resourcephase},); buildRules = (); dependencies = (); name = Maglava; productName = Maglava; productReference = {product}; productType = "com.apple.product-type.application";')
testproduct=obj('testproduct','isa = PBXFileReference; explicitFileType = wrapper.cfbundle; path = MaglavaTests.xctest; sourceTree = BUILT_PRODUCTS_DIR;')
testphase=obj('testphase',f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(tests)},); runOnlyForDeploymentPostprocessing = 0;')
proxy=obj('proxy',f'isa = PBXContainerItemProxy; containerPortal = {uid("project")}; proxyType = 1; remoteGlobalIDString = {target}; remoteInfo = Maglava;')
dep=obj('dependency',f'isa = PBXTargetDependency; target = {target}; targetProxy = {proxy};')
testconfigs=[]
for name in ['Debug','Release']:
    testconfigs.append(obj('test'+name,f'isa = XCBuildConfiguration; name = {name}; buildSettings = {{ PRODUCT_NAME = MaglavaTests; PRODUCT_BUNDLE_IDENTIFIER = dev.fofo.maglava.tests; GENERATE_INFOPLIST_FILE = YES; TEST_HOST = "$(BUILT_PRODUCTS_DIR)/Maglava.app/Maglava"; BUNDLE_LOADER = "$(TEST_HOST)"; HEADER_SEARCH_PATHS = ("$(inherited)", "$(SRCROOT)/../shared"); TARGETED_DEVICE_FAMILY = "1,2"; CODE_SIGN_STYLE = Automatic; SWIFT_VERSION = 5.0; SWIFT_OPTIMIZATION_LEVEL = "-Onone"; }};'))
testlist=obj('testlist',f'isa = XCConfigurationList; buildConfigurations = ({",".join(testconfigs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Debug;')
testtarget=obj('testtarget',f'isa = PBXNativeTarget; buildConfigurationList = {testlist}; buildPhases = ({testphase},); buildRules = (); dependencies = ({dep},); name = MaglavaTests; productName = MaglavaTests; productReference = {testproduct}; productType = "com.apple.product-type.bundle.unit-test";')
obj('project',f'isa = PBXProject; attributes = {{ LastUpgradeCheck = 1640; }}; buildConfigurationList = {uid("projectlist")}; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; knownRegions = (en, Base); mainGroup = {main}; productRefGroup = {products}; projectDirPath = ""; projectRoot = ""; targets = ({target},{testtarget},);')
(project/'project.pbxproj').write_text('// !$*UTF8*$!\n{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {\n'+'\n'.join(objects)+'\n}; rootObject = '+uid('project')+'; }\n')
scheme=project/'xcshareddata/xcschemes';scheme.mkdir(parents=True,exist_ok=True)
ref=f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="Maglava.app" BlueprintName="Maglava" ReferencedContainer="container:Maglava.xcodeproj"/>'
(scheme/'Maglava.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1640" version="1.3"><BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{ref}</BuildActionEntry></BuildActionEntries></BuildAction><TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables><TestableReference skipped="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{testtarget}" BuildableName="MaglavaTests.xctest" BlueprintName="MaglavaTests" ReferencedContainer="container:Maglava.xcodeproj"/></TestableReference></Testables></TestAction><LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable></LaunchAction><ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable></ProfileAction><AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/></Scheme>''')
