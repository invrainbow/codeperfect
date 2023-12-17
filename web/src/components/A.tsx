import React, { AnchorHTMLAttributes, ReactNode } from "react";
import { Link } from "react-router-dom";

function isExternalLink(href: string) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

interface Props extends AnchorHTMLAttributes<HTMLAnchorElement> {
  children: ReactNode;
  href: string;
  newWindow?: boolean;
}

export function A({ children, href, newWindow, ...props }: Props) {
  if (!isExternalLink(href) && !newWindow) {
    return (
      <Link to={href} {...props}>
        {children}
      </Link>
    );
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
  return (
    <a href={href} {...props}>
      {children}
    </a>
  );
}
