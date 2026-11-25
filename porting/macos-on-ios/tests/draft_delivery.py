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

# Exercise the actual HTTP request body and reject an unexpected response.
import io,json,os
os.environ['GH_TOKEN']='unit-test-token'
seen=[]
def response(request,timeout):
 assert request.full_url=='https://api.github.com/repos/fixture/repo/releases'
 assert request.method=='POST' and timeout==30
 data=json.loads(request.data);assert data['draft'] is True and data['target_commitish']==sha
 assert request.get_header('Authorization')=='Bearer unit-test-token'
 seen.append(data)
 return io.BytesIO(json.dumps(dict(release,id=123)).encode())
m.urlopen=response
assert m.create_private_draft('repos/fixture/repo/releases',dict(draft=True,target_commitish=sha))['id']==123
assert len(seen)==1
m.urlopen=lambda *args,**kwargs:io.BytesIO(json.dumps(dict(release,draft=False)).encode())
try:m.create_private_draft('repos/fixture/repo/releases',dict(draft=True,target_commitish=sha))
except RuntimeError:pass
else:raise AssertionError('Unexpected published response accepted')
print('PASS: complete authenticated JSON POST and source/draft validation; no token logging')
