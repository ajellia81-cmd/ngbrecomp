"""Reject published releases and anonymous-readable assets before any IPA upload."""
import importlib.util
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'draft_delivery.py';spec=importlib.util.spec_from_file_location('delivery',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sha='a'*40;release={'draft':True,'target_commitish':sha,'html_url':'https://github.com/fixture/private-draft'};asset={'state':'uploaded','browser_download_url':'https://github.com/fixture/private-asset'}
m.anonymous_status=lambda _: '404';m.prove_private(release,asset,sha)
for bad in [dict(release,draft=False),dict(release,target_commitish='b'*40)]:
 try:m.prove_private(bad,asset,sha)
 except RuntimeError:pass
 else:raise AssertionError('Public or wrong-source release accepted')
for status in ['200','302','403','500']:
 m.anonymous_status=lambda _,s=status:s
 try:m.prove_private(release,asset,sha)
 except RuntimeError:pass
 else:raise AssertionError('Unverified asset privacy accepted: '+status)
print('PASS: draft-only source binding and fail-closed anonymous page/asset privacy checks')
