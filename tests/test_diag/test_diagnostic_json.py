"""Parse diagnostic and CLI output with Python's independent JSON decoder."""
import json
import pathlib
import re
import subprocess
import sys
import tempfile

probe, cli, registry = sys.argv[1:]
result = subprocess.run([probe], capture_output=True, check=True)
assert not result.stderr, result.stderr
records = json.loads(result.stdout.decode('utf-8'))
first = records[0]
assert first['type'] == 'lexical'
assert first['errorCode'] == 'INVALID_CHARACTER'
assert first['errorNumber'] == 0x0109
assert first['code'] == 'detail"\\\n\t\0م😀'
assert first['path'] == 'path"\\\nم'
assert first['source'] == 'نص\t"\\'
assert first['notes'] == [{'line': -12345, 'message': 'note\n"\\'}]
assert first['suggestions'] == ['suggestion\t"\\']
assert first['traceback'][0]['function'] == 'function"\nم'
assert first['traceback'][0]['source'] == first['source']
entries = re.findall(r'^FAIRUZ_ERROR\((\w+), (0x[0-9A-F]+),', pathlib.Path(registry).read_text(), re.M)
assert len(records) == len(entries) + 2
for record, (name, number) in zip(records[1:-1], entries):
    assert record['errorCode'] == name, record
    assert record['errorNumber'] == int(number, 16), record
    assert record['type'] and record['message'], record
assert records[-1]['errorCode'] == 'UNKNOWN'
assert records[-1]['errorNumber'] == 65535
assert records[-1]['path'] == 'bad\ufffd'
assert records[-1]['source'] == '\ufffd'
assert records[-1]['code'] == '\ufffd\ufffd'

with tempfile.TemporaryDirectory(prefix='fairuz-diagnostic-json-') as directory:
    program = pathlib.Path(directory) / 'program.fa'
    for source, name in [(b'\x80', 'INVALID_UTF8'), (('a := ' + '(' * 2000 + '1' + ')' * 2000).encode(), 'EXCEEDED_MAX_NESTING_LIMIT'), ('اذا صحيح\n    اكتب(1)\n'.encode(), 'EXPECTED_COLON_IF')]:
        program.write_bytes(source)
        result = subprocess.run([cli, '--check', '--diagnostics=json', str(program)], capture_output=True)
        assert result.returncode == 65, result
        assert not result.stdout, result.stdout
        diagnostics = json.loads(result.stderr.decode('utf-8'))
        assert any(item['errorCode'] == name for item in diagnostics), diagnostics
        assert all(item['type'] and item['errorCode'] for item in diagnostics)
    # Returned compiler/runtime errors are reported once by the CLI boundary.
    for source, name, options in [
        ('اخرج\n', 'BREAK_OUTSIDE_LOOP', []),
        ('دالة خطأ():\n    اخرج\nخطأ()\n', 'BREAK_OUTSIDE_LOOP', []),
        ('دالة خطأ():\n    اخرج\n', 'BREAK_OUTSIDE_LOOP', ['--check']),
        ('تاكد خطا، "retained detail"\nاكتب("continued")\n', 'ASSERT_FAILED', []),
    ]:
        program.write_text(source, encoding='utf-8')
        result = subprocess.run([cli, '--diagnostics=json', *options, str(program)], capture_output=True)
        assert result.returncode == 65, result
        assert not result.stdout, result.stdout
        diagnostics = json.loads(result.stderr.decode('utf-8'))
        assert len(diagnostics) == 1, diagnostics
        assert diagnostics[0]['errorCode'] == name, diagnostics
        assert diagnostics[0]['path'] == str(program), diagnostics
        if name == 'ASSERT_FAILED':
            assert 'retained detail' in diagnostics[0]['code'], diagnostics
print(f'Parsed {len(records)} diagnostic records and seven CLI failures successfully.')
