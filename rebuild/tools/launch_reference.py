#!/usr/bin/env python3
"""Launch the installed Linux original with an isolated, MSAA-disabled profile.

PECommandLine.cs splits Environment.CommandLine on spaces, requires
-from-launcher, and reads one token beyond every flag. Keep values separate and
end on a value. Before each launch, reset UnityGraphicsQuality to 0 so Unity
starts without MSAA. Game saves and engine preferences use the isolated profile.
This helper does not install the game or change Steam settings.
"""
import argparse
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from xml.dom import minidom
from xml.parsers.expat import ExpatError


CONFIG_RELATIVE = Path('PlanetExplorers/Config/SystemSaveData.xml')


def prepare_settings(user_data):
    """Change only the two graphics settings; return (config, backup or None)."""
    config = user_data / CONFIG_RELATIVE
    original = config.read_bytes() if config.exists() else None
    if original is None:
        document = minidom.parseString(b'<SystemData Version="0.795"/>')
    else:
        try:
            document = minidom.parseString(original)
        except ExpatError as error:
            raise ValueError(f'Invalid settings XML (left unchanged): {config}: {error}') from error
    root = document.documentElement
    if root.tagName != 'SystemData':
        raise ValueError(f'Expected SystemData XML root (left unchanged): {config}')
    changed = original is None
    for name, value in (('QualityLevel', '6'), ('AntiAliasing', '0')):
        nodes = [node for node in root.childNodes
                 if node.nodeType == node.ELEMENT_NODE and node.tagName == name]
        if not nodes:
            node = document.createElement(name)
            root.appendChild(node)
            nodes = [node]
            changed = True
        for node in nodes:
            if node.getAttribute('Index') != value:
                node.setAttribute('Index', value)
                changed = True
    if not changed:
        return config, None

    updated = document.toxml(encoding='utf-8')
    return write_settings_atomically(config, original, updated)


def write_settings_atomically(config, original, updated):
    """Back up exact original bytes before atomically replacing an XML file."""
    config.parent.mkdir(parents=True, exist_ok=True)
    mode = stat.S_IMODE(config.stat().st_mode) if original is not None else 0o600
    backup = None
    if original is not None:
        fd, name = tempfile.mkstemp(prefix=config.name + '.before-reference-',
                                    suffix='.bak', dir=config.parent)
        backup = Path(name)
        with os.fdopen(fd, 'wb') as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
    fd, name = tempfile.mkstemp(prefix='.' + config.name + '.', dir=config.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, 'wb') as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(updated)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, config)
    finally:
        temporary.unlink(missing_ok=True)
    return config, backup


def prepare_unity_preferences(config_home=None):
    """Reset only the engine startup quality, before Unity initializes GL."""
    if config_home is None:
        config_home = Path(os.environ.get('XDG_CONFIG_HOME') or Path.home() / '.config')
        # The XDG specification requires an absolute path; ignore invalid values.
        if not config_home.is_absolute():
            config_home = Path.home() / '.config'
    prefs = config_home / 'unity3d/Pathea Games/Planet Explorers/prefs'
    original = prefs.read_bytes() if prefs.exists() else None
    try:
        document = minidom.parseString(original if original is not None else
                                      b'<unity_prefs version_major="1" version_minor="1"/>')
    except ExpatError as error:
        raise ValueError(f'Invalid Unity preferences XML (left unchanged): {prefs}: {error}') from error
    root = document.documentElement
    if root.tagName != 'unity_prefs':
        raise ValueError(f'Expected unity_prefs XML root (left unchanged): {prefs}')
    nodes = [node for node in root.childNodes if node.nodeType == node.ELEMENT_NODE
             and node.tagName == 'pref' and node.getAttribute('name') == 'UnityGraphicsQuality']
    changed = original is None
    if not nodes:
        node = document.createElement('pref')
        node.setAttribute('name', 'UnityGraphicsQuality')
        root.appendChild(node)
        nodes = [node]
        changed = True
    for node in nodes:
        if any(child.nodeType == child.ELEMENT_NODE for child in node.childNodes):
            raise ValueError(f'Invalid UnityGraphicsQuality value (left unchanged): {prefs}')
        text_nodes = [child for child in node.childNodes
                      if child.nodeType in (child.TEXT_NODE, child.CDATA_SECTION_NODE)]
        value = ''.join(child.data for child in text_nodes)
        if node.getAttribute('type') != 'int':
            node.setAttribute('type', 'int')
            changed = True
        if value.strip() != '0':
            for child in text_nodes:
                node.removeChild(child)
            node.appendChild(document.createTextNode('0'))
            changed = True
    if not changed:
        return prefs, None
    return write_settings_atomically(prefs, original, document.toxml(encoding='utf-8'))


def launch_command(game_dir, user_data):
    executable = game_dir / 'PE_Client.x86_64'
    plugins = game_dir / 'PE_Client_Data/Plugins/x86_64'
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError(f'Native executable is missing or not executable: {executable}')
    if not plugins.is_dir():
        raise ValueError(f'Native plugin directory is missing: {plugins}')
    command = [str(executable), '-from-launcher', '-rw', '1280', '-rh', '720',
               '-ql', '0', '-language', 'English', '-ocl', 'Cpu',
               '-userpath', str(user_data), '-screen-fullscreen', '0']
    environment = os.environ.copy()
    environment.update(SteamAppId='237870', SteamGameId='237870', LANG='en',
                       XDG_CONFIG_HOME=str(user_data / 'engine-config'))
    previous = environment.get('LD_LIBRARY_PATH', '')
    environment['LD_LIBRARY_PATH'] = str(plugins) + (':' + previous if previous else '')
    return command, environment


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path,
                        default=Path.home() / '.local/share/Steam/steamapps/common/Planet Explorers',
                        help='installed original Linux game directory')
    parser.add_argument('--user-data', type=Path,
                        default=Path.home() / '.local/share/planet-explorers-reference',
                        help='profile root without whitespace; created if needed')
    parser.add_argument('--prepare-only', action='store_true',
                        help='validate installation and prepare settings without launching')
    args = parser.parse_args(argv)
    try:
        game_dir = args.game_dir.expanduser().resolve()
        user_data = args.user_data.expanduser().resolve()
        if any(character.isspace() for character in str(user_data)):
            raise ValueError('The original game cannot parse whitespace in --user-data paths; '
                             'choose a path without whitespace.')
        command, environment = launch_command(game_dir, user_data)
        config, backup = prepare_settings(user_data)
        prefs, prefs_backup = prepare_unity_preferences(user_data / 'engine-config')
        print(f'Reference profile: {user_data}', flush=True)
        print(f'Settings: {config} (QualityLevel=6, AntiAliasing=0)', flush=True)
        if backup is not None:
            print(f'Previous settings backup: {backup}', flush=True)
        print(f'Isolated Unity startup preferences: {prefs} (UnityGraphicsQuality=0)', flush=True)
        if prefs_backup is not None:
            print(f'Previous Unity preferences backup: {prefs_backup}', flush=True)
        if args.prepare_only:
            return 0
        return_code = subprocess.run(command, cwd=game_dir, env=environment).returncode
        return return_code if return_code >= 0 else 128 - return_code
    except (OSError, ValueError) as error:
        print(f'Reference launch failed: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    sys.exit(main())
