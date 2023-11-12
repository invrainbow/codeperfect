import React from "react";
import Link from "next/link";

function isExternalLink(href) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

export function A({ children, href, newWindow, ...props }) {
  if (href && !isExternalLink(href) && !newWindow) {
    props.href = href;
    return <Link {...props}>{children}</Link>;
  }

  let useNewWindow;
  if (newWindow) {
    useNewWindow = true;
  } else if (newWindow === false) {
    useNewWindow = false;
  } else if (isExternalLink(href) && !href.startsWith("mailto:")) {
    useNewWindow = true;
  }

  if (useNewWindow) {
    props.target = "_blank";
  }
  props.href = href;
  return <a {...props}>{children}</a>;
}

export function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}
