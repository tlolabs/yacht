#!/usr/bin/env python3
"""Deterministic Xcode app/UI-test project; no third-party project generator."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
objects = {}
def uid(name): return hashlib.sha1(name.encode()).hexdigest()[:24].upper()
def add(identifier, **values):
    key = uid(identifier); objects[key] = values; return key
def serialize(value, depth=0):
    if isinstance(value, dict): return '{\n' + ''.join('\t'*(depth+1)+json.dumps(k)+' = '+serialize(v,depth+1)+';\n' for k,v in value.items()) + '\t'*depth+'}'
    if isinstance(value, list): return '(' + ', '.join(serialize(v,depth) for v in value) + ')'
    return json.dumps(str(value))
app_id, test_id, project_id = uid('app'), uid('tests'), uid('project')
product_app = add('product-app', isa='PBXFileReference', explicitFileType='wrapper.application', path='YACHT.app', sourceTree='BUILT_PRODUCTS_DIR')
product_test = add('product-tests', isa='PBXFileReference', explicitFileType='wrapper.cfbundle', path='YachtUITests.xctest', sourceTree='BUILT_PRODUCTS_DIR')
package = add('package', isa='XCLocalSwiftPackageReference', relativePath='.')
core_product = add('core-product', isa='XCSwiftPackageProductDependency', package=package, productName='YachtCore')
core_build = add('core-build', isa='PBXBuildFile', productRef=core_product)
groups = []
phases = {}
for target, folder in [('app', 'Sources/YachtApp'), ('tests', 'UITests')]:
    refs, builds = [], []
    for path in sorted((root/folder).rglob('*.swift')):
        relative = str(path.relative_to(root))
        ref = add(relative, isa='PBXFileReference', lastKnownFileType='sourcecode.swift', path=relative, sourceTree='SOURCE_ROOT')
        refs.append(ref); builds.append(add(relative+'-build', isa='PBXBuildFile', fileRef=ref))
    groups.append(add(target+'-group', isa='PBXGroup', name=folder, children=refs, sourceTree='<group>'))
    sources = add(target+'-sources', isa='PBXSourcesBuildPhase', buildActionMask=2147483647, files=builds, runOnlyForDeploymentPostprocessing=0)
    frameworks = add(target+'-frameworks', isa='PBXFrameworksBuildPhase', buildActionMask=2147483647, files=[core_build] if target=='app' else [], runOnlyForDeploymentPostprocessing=0)
    phases[target] = [sources, frameworks]
license_ref = add('license-ref', isa='PBXFileReference', lastKnownFileType='text', path='LICENSE', sourceTree='SOURCE_ROOT')
license_build = add('license-build', isa='PBXBuildFile', fileRef=license_ref)
phases['app'].insert(0, add('rust-build', isa='PBXShellScriptBuildPhase', buildActionMask=2147483647, files=[], inputPaths=[], outputPaths=[], shellPath='/bin/bash', shellScript='cd "$SRCROOT" && ./script/build_rust_macos.sh', alwaysOutOfDate=1, runOnlyForDeploymentPostprocessing=0))
phases['app'].append(add('app-resources', isa='PBXResourcesBuildPhase', buildActionMask=2147483647, files=[license_build], runOnlyForDeploymentPostprocessing=0))
groups.append(license_ref)
products = add('products', isa='PBXGroup', name='Products', children=[product_app, product_test], sourceTree='<group>')
main = add('main-group', isa='PBXGroup', children=groups+[products], sourceTree='<group>')
def configs(name, common):
    refs = []
    for configuration in ['Debug', 'Release']:
        settings = common | {'SWIFT_OPTIMIZATION_LEVEL': '-Onone' if configuration=='Debug' else '-O', 'DEBUG_INFORMATION_FORMAT': 'dwarf' if configuration=='Debug' else 'dwarf-with-dsym'}
        if configuration=='Debug':
            settings['SWIFT_ACTIVE_COMPILATION_CONDITIONS']='DEBUG'
            settings['ONLY_ACTIVE_ARCH']='YES'
        refs.append(add(name+configuration, isa='XCBuildConfiguration', name=configuration, buildSettings=settings))
    return add(name+'config-list', isa='XCConfigurationList', buildConfigurations=refs, defaultConfigurationIsVisible=0, defaultConfigurationName='Release')
base = {'ARCHS':'$(ARCHS_STANDARD)', 'MACOSX_DEPLOYMENT_TARGET':'14.0', 'SDKROOT':'macosx', 'SWIFT_VERSION':'6.0', 'CLANG_ENABLE_MODULES':'YES', 'CODE_SIGN_IDENTITY':'-', 'CODE_SIGN_STYLE':'Manual', 'ENABLE_HARDENED_RUNTIME':'YES', 'SWIFT_STRICT_CONCURRENCY':'complete', 'ENABLE_USER_SCRIPT_SANDBOXING':'NO', 'SUPPORTED_PLATFORMS':'macosx'}
project_configs=configs('project',base)
app_configs=configs('app', {'PRODUCT_NAME':'YACHT', 'EXECUTABLE_NAME':'YachtApp', 'PRODUCT_MODULE_NAME':'YachtNativeApp', 'PRODUCT_BUNDLE_IDENTIFIER':'com.local.yacht.csvhtmltranslator', 'INFOPLIST_FILE':'Support/Info.plist', 'LD_RUNPATH_SEARCH_PATHS':['$(inherited)', '@executable_path/../Frameworks']})
test_configs=configs('tests', {'ENABLE_HARDENED_RUNTIME':'NO', 'PRODUCT_NAME':'YachtUITests', 'PRODUCT_BUNDLE_IDENTIFIER':'com.local.yacht.uitests', 'GENERATE_INFOPLIST_FILE':'YES', 'TEST_TARGET_NAME':'Yacht', 'LD_RUNPATH_SEARCH_PATHS':['$(inherited)', '@executable_path/../Frameworks', '@loader_path/../Frameworks']})
proxy=add('test-proxy',isa='PBXContainerItemProxy',containerPortal=project_id,proxyType=1,remoteGlobalIDString=app_id,remoteInfo='Yacht')
dep=add('test-dependency',isa='PBXTargetDependency',target=app_id,targetProxy=proxy)
add('app', isa='PBXNativeTarget', buildConfigurationList=app_configs, buildPhases=phases['app'], buildRules=[], dependencies=[], name='Yacht', productName='YACHT', productReference=product_app, productType='com.apple.product-type.application', packageProductDependencies=[core_product])
add('tests', isa='PBXNativeTarget', buildConfigurationList=test_configs, buildPhases=phases['tests'], buildRules=[], dependencies=[dep], name='YachtUITests', productName='YachtUITests', productReference=product_test, productType='com.apple.product-type.bundle.ui-testing')
add('project', isa='PBXProject', attributes={'LastUpgradeCheck':'2600', 'BuildIndependentTargetsInParallel':'YES', 'TargetAttributes':{test_id:{'TestTargetID':app_id}}}, buildConfigurationList=project_configs, compatibilityVersion='Xcode 14.0', developmentRegion='en', knownRegions=['en','Base'], mainGroup=main, productRefGroup=products, projectDirPath='', projectRoot='', targets=[app_id,test_id], packageReferences=[package])
p=root/'Yacht.xcodeproj';p.mkdir(exist_ok=True)
(p/'project.pbxproj').write_text('// !$*UTF8*$!\n'+serialize({'archiveVersion':1,'classes':{},'objectVersion':56,'objects':objects,'rootObject':project_id})+'\n')
scheme=p/'xcshareddata/xcschemes/Yacht.xcscheme';scheme.parent.mkdir(parents=True,exist_ok=True)
def buildable(identifier,name):return f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{identifier}" BuildableName="{name}" BlueprintName="{("Yacht" if identifier==app_id else "YachtUITests")}" ReferencedContainer="container:Yacht.xcodeproj"/>'
scheme.write_text(f"""<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="2600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{buildable(app_id,'YACHT.app')}</BuildActionEntry></BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables><TestableReference skipped="NO" parallelizable="NO">{buildable(test_id,'YachtUITests.xctest')}</TestableReference></Testables></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0">{buildable(app_id,'YACHT.app')}</BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0">{buildable(app_id,'YACHT.app')}</BuildableProductRunnable></ProfileAction>
<AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
""")
print('Generated Yacht.xcodeproj')
