import dom, { AProps } from "./dom";

export interface Props extends AProps {
  newWindow?: boolean;
}

export const Link = ({ newWindow = true, ...rest }: Props) => (
  <dom.a {...rest} target={newWindow ? "_blank" : rest.target} />
);
