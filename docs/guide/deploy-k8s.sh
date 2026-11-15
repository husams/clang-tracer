#!/usr/bin/env bash
# Publish docs/guide/index.html on a Kubernetes cluster behind an Istio gateway.
# Everything is sent in one server-side `kubectl apply`: two ConfigMaps (the
# page and the nginx configuration), a Deployment, a Service and a
# VirtualService. ConfigMap names end in a content hash, so a changed page
# rolls the Deployment and an unchanged page is a no-op.
set -euo pipefail

guide_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
name=clang-tracer-guide
namespace=docs
host=; gateway=; dns_target=; dry_run=

usage() {
  cat <<'TXT'
Usage: deploy-k8s.sh [--namespace NS] [--name NAME] [--host HOST]
                     [--gateway NS/GATEWAY] [--dns-target IP] [--dry-run]
  --namespace NS      Namespace to deploy into (default: docs)
  --name NAME         Name of every resource (default: clang-tracer-guide)
  --host HOST         Public host name routed to the guide
  --gateway NS/NAME   Istio gateway that serves the host
  --dns-target IP     Optional external-dns target annotation
  --dry-run           Validate on the server and print the result; change nothing
--host, --gateway and --dns-target default to the values of the VirtualService
that is already deployed, so an update needs no options. The first deployment
needs --host and --gateway. Uses the current kubectl context.
TXT
}

while (($#)); do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --dry-run) dry_run=1; shift; continue ;;
  esac
  [[ $# -ge 2 && -n "$2" ]] || { echo "Missing value for $1" >&2; exit 2; }
  case "$1" in
    --namespace) namespace=$2 ;;
    --name) name=$2 ;;
    --host) host=$2 ;;
    --gateway) gateway=$2 ;;
    --dns-target) dns_target=$2 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift 2
done

page="$guide_dir/index.html"
[[ -f "$page" ]] || { echo "Missing $page. Run docs/guide/build.py." >&2; exit 2; }

# Reuse the routing of the deployed VirtualService unless it was given.
deployed() {
  kubectl -n "$namespace" get virtualservice "$name" -o "jsonpath=$1" 2>/dev/null || true
}
[[ -n "$host" ]] || host=$(deployed '{.spec.hosts[0]}')
[[ -n "$gateway" ]] || gateway=$(deployed '{.spec.gateways[0]}')
[[ -n "$dns_target" ]] ||
  dns_target=$(deployed '{.metadata.annotations.external-dns\.alpha\.kubernetes\.io/target}')
[[ -n "$host" && -n "$gateway" ]] ||
  { echo 'The first deployment needs --host and --gateway.' >&2; exit 2; }

# Recorded on the pod template: which source revision the page came from.
revision=unknown
if git -C "$guide_dir" rev-parse --git-dir >/dev/null 2>&1; then
  revision="$(git -C "$guide_dir" rev-parse --abbrev-ref HEAD)@$(git -C "$guide_dir" rev-parse --short HEAD)"
  git -C "$guide_dir" diff --quiet HEAD -- "$page" || revision+="-dirty"
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat >"$work/default.conf" <<'CONF'
server {
    listen 8080;
    server_name _;
    root /usr/share/nginx/html;
    index index.html;

    gzip on;
    gzip_min_length 1024;
    gzip_comp_level 5;
    gzip_types text/plain text/css application/javascript application/json image/svg+xml;

    location = /healthz {
        access_log off;
        default_type text/plain;
        return 200 "ok\n";
    }

    location / {
        try_files $uri $uri/ =404;
        add_header Cache-Control "no-cache";
    }
}
CONF

content=$(sha256sum "$page" | cut -c1-10)
site_map="$name-site-$content"
nginx_map="$name-nginx-$(sha256sum "$work/default.conf" | cut -c1-10)"

config_map() { # NAME KEY=FILE
  kubectl create configmap "$1" -n "$namespace" --from-file="$2" --dry-run=client -o yaml |
    kubectl label --local -f - -o yaml \
      "app.kubernetes.io/name=$name" app.kubernetes.io/part-of=docs
}

dns_annotation=
[[ -z "$dns_target" ]] || dns_annotation="
  annotations:
    external-dns.alpha.kubernetes.io/target: \"$dns_target\""

{
  config_map "$site_map" "index.html=$page"
  echo ---
  config_map "$nginx_map" "default.conf=$work/default.conf"
  cat <<YAML
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: $name
  namespace: $namespace
  labels:
    app.kubernetes.io/component: web
    app.kubernetes.io/name: $name
    app.kubernetes.io/part-of: docs
spec:
  replicas: 1
  revisionHistoryLimit: 3
  selector:
    matchLabels:
      app.kubernetes.io/name: $name
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  template:
    metadata:
      annotations:
        clang-tracer-guide/content: "$content"
        clang-tracer-guide/revision: "$revision"
      labels:
        app.kubernetes.io/component: web
        app.kubernetes.io/name: $name
    spec:
      automountServiceAccountToken: false
      securityContext:
        fsGroup: 101
        runAsGroup: 101
        runAsNonRoot: true
        runAsUser: 101
        seccompProfile:
          type: RuntimeDefault
      containers:
      - name: nginx
        image: nginxinc/nginx-unprivileged:1.29-alpine
        imagePullPolicy: IfNotPresent
        ports:
        - name: http
          containerPort: 8080
          protocol: TCP
        readinessProbe:
          httpGet:
            path: /healthz
            port: http
          initialDelaySeconds: 2
          periodSeconds: 10
        livenessProbe:
          httpGet:
            path: /healthz
            port: http
          initialDelaySeconds: 10
          periodSeconds: 30
        resources:
          requests:
            cpu: 10m
            memory: 32Mi
          limits:
            memory: 128Mi
        securityContext:
          allowPrivilegeEscalation: false
          capabilities:
            drop:
            - ALL
          readOnlyRootFilesystem: true
        volumeMounts:
        - name: site
          mountPath: /usr/share/nginx/html
          readOnly: true
        - name: nginx-conf
          mountPath: /etc/nginx/conf.d
          readOnly: true
        - name: tmp
          mountPath: /tmp
      volumes:
      - name: site
        configMap:
          name: $site_map
      - name: nginx-conf
        configMap:
          name: $nginx_map
      - name: tmp
        emptyDir:
          sizeLimit: 16Mi
---
apiVersion: v1
kind: Service
metadata:
  name: $name
  namespace: $namespace
  labels:
    app.kubernetes.io/component: web
    app.kubernetes.io/name: $name
    app.kubernetes.io/part-of: docs
spec:
  selector:
    app.kubernetes.io/name: $name
  ports:
  - name: http
    port: 80
    protocol: TCP
    targetPort: http
---
apiVersion: networking.istio.io/v1
kind: VirtualService
metadata:
  name: $name
  namespace: $namespace$dns_annotation
  labels:
    app.kubernetes.io/component: web
    app.kubernetes.io/name: $name
    app.kubernetes.io/part-of: docs
spec:
  gateways:
  - $gateway
  hosts:
  - $host
  http:
  - route:
    - destination:
        host: $name.$namespace.svc.cluster.local
        port:
          number: 80
YAML
} >"$work/manifest.yaml"

# The one apply. --force-conflicts takes back fields changed by hand.
apply=(kubectl apply --server-side --force-conflicts -f "$work/manifest.yaml")
if [[ -n "$dry_run" ]]; then
  "${apply[@]}" --dry-run=server
  exit 0
fi
"${apply[@]}"
kubectl -n "$namespace" rollout status "deployment/$name" --timeout=180s

# Drop page and nginx ConfigMaps that no retained ReplicaSet can roll back to.
in_use=$(kubectl -n "$namespace" get replicasets -l "app.kubernetes.io/name=$name" \
  -o 'jsonpath={range .items[*].spec.template.spec.volumes[*]}{.configMap.name}{"\n"}{end}')
for map in $(kubectl -n "$namespace" get configmaps -l "app.kubernetes.io/name=$name" -o name); do
  grep -qxF "${map#configmap/}" <<<"$in_use" || kubectl -n "$namespace" delete "$map"
done
echo "Deployed $revision ($content) at https://$host/"
