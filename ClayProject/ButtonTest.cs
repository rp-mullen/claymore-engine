using ClaymoreEngine;
using System;

public class ButtonTest : ScriptComponent
{
    public override void OnCreate()
    {
        var e = new Entity(EntityID);
        var button = e.GetComponent<Button>();

        if (button != null)
        {
            Console.WriteLine("[ButtonTest] Button Found!");
            button.OnClicked += () => Console.WriteLine("Button Clicked");
        }
    }
}