import { AnchorHTMLAttributes } from "react";

export const A = (props: AnchorHTMLAttributes<HTMLAnchorElement>) => (
  <a target="_blank" {...props} />
);
